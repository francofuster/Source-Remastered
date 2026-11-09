/*
===========================================================================
cl_touch.c -- on-screen touch controls (Android, USE_TOUCH)

Every control ends in the same usercmd_t the keyboard and the mouse fill on
PC: bg_pmove, the bots and network games keep their rules, and the game
modules know nothing about touch.

  - a floating stick in the left half (move, walk, boost)
  - the right half turns the camera; dragging off KI or FUERTE does too
  - KI, FUERTE, Z, GUARDIA, LOCK, AURA, TECNICA, VUELO and pause
  - in menus a finger is an absolute cursor plus the left mouse button

Phase F3 adds the contexts: the same buttons do what the situation calls
for (free, locked on, melee, opponent launched, launched yourself, beam
struggle), with their labels, the guided combo, the launch direction on the
stick, the Z hints and the aim assist (cl_aimassist.c).
Layout and thresholds: the Android proposal, section "Joystick virtual".
===========================================================================
*/

#ifdef USE_TOUCH

#include "client.h"

#define TOUCH_MAX_FINGERS		10

// Stick, in dp and fractions of its radius
#define STICK_RADIUS			62.0f
#define STICK_KNOB				26.0f
#define STICK_DEAD				0.12f
#define STICK_WALK				0.50f
#define STICK_BOOST				1.20f
#define STICK_BOOST_MS			120
#define STICK_SECTOR			0.38f
#define STICK_IDLE_X			132.0f	// from the left
#define STICK_IDLE_Y			114.0f	// from the bottom
#define EDGE_MARGIN				24.0f	// Android keeps the screen edges for its gestures

#define JUMP_TAP_MS				180		// VUELO up: shorter is a jump, longer flies up
#define AURA_SWIPE				28.0f	// dp
#define AURA_CHARGE_MS			150
#define SKILL_STEP				36.0f	// dp of vertical drag per technique
#define LAND_TRACE				160.0f	// VUELO down this close to the ground lands

// Guided combo (Asistido, in melee): holding KI rushes with W, then holds
// FUERTE past 550 ms for the power launch, over and over. A tap still breaks.
#define COMBO_HOLD_MS			220
#define COMBO_RUSH_MS			900
#define COMBO_LAUNCH_MS			650
#define COMBO_CYCLE_MS			1650

// Z hints, from bg_pmove.c (PM_LockedZanzoken)
#define ZAN_DODGE_RANGE			200
#define ZAN_CHASE_RANGE			4000
#define ZAN_CHASE_MAX			3
#define DODGE_CLOSING_SPEED		300.0f

typedef enum {
	CTX_FREE,
	CTX_LOCKED,
	CTX_MELEE,
	CTX_FOE_LAUNCHED,	// the opponent flies from a knockback
	CTX_KNOCKED,		// we fly from a knockback
	CTX_STRUGGLE		// beam struggle
} touchContext_t;

typedef enum {
	TL_NONE,
	TL_KI, TL_STRONG, TL_Z, TL_GUARD, TL_LOCK, TL_AURA, TL_SKILL,
	TL_COMBO, TL_BREAK, TL_LAUNCH, TL_COUNTER, TL_DODGE, TL_CHASE, TL_BACK,
	TL_BRAKE, TL_RECOVER, TL_PUSH,
	TL_COUNT
} touchLabel_t;

static const char *touchLabelImages[TL_COUNT] = {
	NULL,
	"ki", "strong", "z", "guard", "lock", "aura", "skill",
	"combo", "break", "launch", "counter", "dodge", "chase", "back",
	"brake", "recover", "push"
};

typedef enum {
	TB_KI,
	TB_STRONG,
	TB_Z,
	TB_GUARD,
	TB_LOCK,
	TB_AURA,
	TB_SKILL,
	TB_FLYUP,
	TB_FLYDOWN,
	TB_PAUSE,
	TB_PUSH,			// only in a beam struggle
	TB_COUNT
} touchButtonId_t;

typedef struct {
	touchLabel_t	image;	// label in the free context, TL_NONE for none
	int			bits;		// usercmd buttons while held
	float		x, y;		// center in dp: x from the right edge, y from the bottom
							// (from the top when fromTop, x ignored when centerX)
	float		radius;		// dp; 0 for the rectangular halves of VUELO
	float		halfW, halfH;	// dp, rectangular buttons
	qboolean	fromTop;
	qboolean	centerX;
	qboolean	dragLook;	// dragging off it turns the camera, the button stays held
	float		tint[3];

	// runtime
	float		px, py, pr, phw, phh;	// in pixels
	int			finger;		// index in tc.fingers, -1 when up
	int			downTime;
	qboolean	latched;	// pressed and released before the next usercmd
} touchButton_t;

static touchButton_t touchButtons[TB_COUNT] = {
	//            image      bits                 x      y      r    hw    hh   top     cx     look
	/* KI      */ { TL_KI,     BUTTON_ATTACK,      92,    92,   44,   0,    0,  qfalse, qfalse, qtrue,  { 1.00f, 0.62f, 0.15f } },
	/* FUERTE  */ { TL_STRONG, BUTTON_ALT_ATTACK, 188,    72,   36,   0,    0,  qfalse, qfalse, qtrue,  { 1.00f, 0.32f, 0.26f } },
	/* Z       */ { TL_Z,      BUTTON_TELEPORT,   162,   164,   36,   0,    0,  qfalse, qfalse, qfalse, { 0.30f, 0.86f, 1.00f } },
	/* GUARDIA */ { TL_GUARD,  BUTTON_BLOCK,       68,   192,   34,   0,    0,  qfalse, qfalse, qfalse, { 0.45f, 0.58f, 1.00f } },
	/* LOCK    */ { TL_LOCK,   BUTTON_GESTURE,     58,   130,   28,   0,    0,  qtrue,  qfalse, qfalse, { 1.00f, 0.86f, 0.30f } },
	/* AURA    */ { TL_AURA,   BUTTON_POWERLEVEL, 268,   194,   28,   0,    0,  qfalse, qfalse, qfalse, { 0.75f, 0.85f, 1.00f } },
	/* TECNICA */ { TL_SKILL,  0,                 160,   254,   27,   0,    0,  qfalse, qfalse, qfalse, { 0.45f, 0.95f, 0.55f } },
	/* VUELO + */ { TL_NONE,   0,                 268,   122,    0,  27,   28,  qfalse, qfalse, qfalse, { 1.00f, 1.00f, 1.00f } },
	/* VUELO - */ { TL_NONE,   0,                 268,    66,    0,  27,   28,  qfalse, qfalse, qfalse, { 1.00f, 1.00f, 1.00f } },
	/* pause   */ { TL_NONE,   0,                   0,    32,   20,   0,    0,  qtrue,  qtrue,  qfalse, { 1.00f, 1.00f, 1.00f } },
	/* EMPUJAR */ { TL_PUSH,   BUTTON_BOOST,      132,   132,   84,   0,    0,  qfalse, qfalse, qfalse, { 1.00f, 0.78f, 0.25f } },
};

typedef enum {
	TF_NONE,		// ignored until it lifts
	TF_STICK,
	TF_LOOK,
	TF_BUTTON,
	TF_MENU
} touchFingerKind_t;

typedef struct {
	qboolean			active;
	long long			id;
	touchFingerKind_t	kind;
	int					button;
	float				x, y;		// pixels
	float				x0, y0;		// where it went down
	float				lastX, lastY;
	int					downTime;
	qboolean			looking;	// a dragLook finger that left its button
} touchFinger_t;

static struct {
	qboolean		mediaRegistered;
	touchFinger_t	fingers[TOUCH_MAX_FINGERS];
	float			dp;					// pixels per dp
	float			w, h;

	int				stickFinger;
	float			stickX, stickY;		// base, pixels
	int				boostOverSince;
	qboolean		boost;

	float			lookX, lookY;		// dp waiting for the next usercmd

	int				auraMode;			// AURA_*
	float			skillDrag;			// dp since the last technique step
	qboolean		skillStepped;

	float			stickNX, stickNY, stickMag;	// last stick direction and travel
	touchContext_t	ctx;
	qboolean		zHint;				// Z is worth pressing now
	qboolean		chaseWindow;		// after a chase: up catches, FUERTE launches back
	qboolean		lockPressed;
	qboolean		lockSendNext;		// LOCK turned the view: the button goes next command

	qhandle_t		disc, ring, glow, knob, stickBase, pill, arrowUp, arrowDown, pause;
	qhandle_t		labels[TL_COUNT];
} tc;

enum { AURA_NONE, AURA_WAIT, AURA_CHARGE, AURA_TIER_UP, AURA_TIER_DOWN };

static cvar_t	*touch_enable;
static cvar_t	*touch_alpha;
static cvar_t	*touch_scale;
static cvar_t	*touch_lookSpeed;
static cvar_t	*touch_debug;
static cvar_t	*touch_profile;		// registered by cl_aimassist.c

/*
=================
Touch_LockedEntity

The opponent we are locked on, from the last snapshot; NULL if not there.
=================
*/
static entityState_t *Touch_LockedEntity( void ) {
	int num = cl.snap.ps.lockedTarget - 1;
	int i;

	if ( num < 0 ) {
		return NULL;
	}
	for ( i = 0; i < cl.snap.numEntities; i++ ) {
		entityState_t *es = &cl.parseEntities[ ( cl.snap.parseEntitiesNum + i ) & ( MAX_PARSE_ENTITIES - 1 ) ];

		if ( es->number == num ) {
			return es;
		}
	}
	return NULL;
}

/*
=================
Touch_UpdateContext

What the situation is, from our playerState and the locked opponent. The
client does not see the melee state of the opponent, so the Z hints are
approximations of the windows PM_LockedZanzoken opens.
=================
*/
static void Touch_UpdateContext( void ) {
	playerState_t *ps = &cl.snap.ps;
	entityState_t *foe;
	qboolean rearmed;
	vec3_t line;
	float dist = 0.0f;

	tc.ctx = CTX_FREE;
	tc.zHint = qfalse;
	tc.chaseWindow = qfalse;
	if ( !cl.snap.valid ) {
		return;
	}

	foe = Touch_LockedEntity();
	if ( foe ) {
		VectorSubtract( foe->pos.trBase, ps->origin, line );
		dist = VectorLength( line );
	}
	rearmed = ps->timers[tmZanzokenRearm] <= 0;
	tc.chaseWindow = ps->timers[tmZanzokenChase] > 0;

	if ( ps->bitFlags & isStruggling ) {
		tc.ctx = CTX_STRUGGLE;
	} else if ( ps->timers[tmKnockback] > 0 ) {
		tc.ctx = CTX_KNOCKED;
	} else if ( ps->bitFlags & usingMelee ) {
		int state = ps->stats[stMeleeState];

		tc.ctx = CTX_MELEE;
		// being hit, not hitting back: a counter takes us behind it
		tc.zHint = rearmed && ( state == stMeleeStartHit || state == stMeleeIdle || state == stMeleeStartDodge );
	} else if ( foe && ( foe->legsAnim & ~ANIM_TOGGLEBIT ) == ANIM_KNOCKBACK ) {
		tc.ctx = CTX_FOE_LAUNCHED;
		tc.zHint = ( ps->stats[stZanzokenChain] & ZC_CHASES ) < ZAN_CHASE_MAX && dist <= ZAN_CHASE_RANGE;
	} else if ( ps->lockedTarget > 0 ) {
		tc.ctx = CTX_LOCKED;
		// it rushes at us: Z dodges behind it
		if ( foe && rearmed && !ps->timers[tmFreeze] && dist > 0.0f && dist <= ZAN_DODGE_RANGE ) {
			VectorNormalize( line );
			tc.zHint = -DotProduct( foe->pos.trDelta, line ) > DODGE_CLOSING_SPEED;
		}
	}
}

/*
=================
Touch_Visible

Buttons that exist in the current context.
=================
*/
static qboolean Touch_Visible( int button ) {
	if ( tc.ctx == CTX_STRUGGLE ) {
		return button == TB_PUSH || button == TB_PAUSE;
	}
	// GUARD only with a lock-on, where blocking matters
	if ( button == TB_GUARD ) {
		return cl.snap.valid && cl.snap.ps.lockedTarget > 0;
	}
	return button != TB_PUSH;
}

/*
=================
Touch_Label

What a button says in the current context; *dim when it does nothing there.
=================
*/
static touchLabel_t Touch_Label( int button, qboolean *dim ) {
	qboolean assisted = touch_profile && touch_profile->integer;

	*dim = qfalse;
	switch ( button ) {
	case TB_KI:
		switch ( tc.ctx ) {
		case CTX_MELEE:			return assisted ? TL_COMBO : TL_BREAK;
		case CTX_KNOCKED:		return TL_BRAKE;
		case CTX_FOE_LAUNCHED:	*dim = qtrue; return TL_KI;
		default:				return TL_KI;
		}
	case TB_STRONG:
		if ( tc.chaseWindow ) {
			return TL_BACK;
		}
		switch ( tc.ctx ) {
		case CTX_MELEE:			return TL_LAUNCH;
		case CTX_KNOCKED:		return TL_RECOVER;
		case CTX_FOE_LAUNCHED:	*dim = qtrue; return TL_STRONG;
		default:				return TL_STRONG;
		}
	case TB_Z:
		switch ( tc.ctx ) {
		case CTX_MELEE:			return TL_COUNTER;
		case CTX_KNOCKED:		return TL_COUNTER;
		case CTX_FOE_LAUNCHED:	return TL_CHASE;
		case CTX_LOCKED:		return tc.zHint ? TL_DODGE : TL_Z;
		default:				return TL_Z;
		}
	case TB_GUARD:
		*dim = tc.ctx == CTX_KNOCKED || tc.ctx == CTX_FOE_LAUNCHED;
		return TL_GUARD;
	default:
		return touchButtons[button].image;
	}
}

/*
=================
Touch_Layout

Converts the dp layout to pixels for the current screen.
=================
*/
static void Touch_Layout( void ) {
	float dpi = IN_DisplayDPI();
	float dp;
	int i;

	tc.w = cls.glconfig.vidWidth;
	tc.h = cls.glconfig.vidHeight;

	// Real size when the screen reports its density; never so big that the
	// right cluster (about 360 dp tall) leaves the screen
	dp = ( dpi > 0.0f ) ? dpi / 160.0f : tc.h / 390.0f;
	if ( dp > tc.h / 360.0f ) {
		dp = tc.h / 360.0f;
	}
	dp *= Com_Clamp( 0.6f, 1.6f, touch_scale->value );
	tc.dp = dp;

	for ( i = 0; i < TB_COUNT; i++ ) {
		touchButton_t *b = &touchButtons[i];

		b->px = b->centerX ? tc.w * 0.5f : tc.w - b->x * dp;
		b->py = b->fromTop ? b->y * dp : tc.h - b->y * dp;
		b->pr = b->radius * dp;
		b->phw = b->halfW * dp;
		b->phh = b->halfH * dp;
	}
}

/*
=================
Touch_HitButton
=================
*/
static int Touch_HitButton( float x, float y ) {
	int i;
	int best = -1;
	float bestDist = 0.0f;

	for ( i = 0; i < TB_COUNT; i++ ) {
		touchButton_t *b = &touchButtons[i];
		float dx = x - b->px;
		float dy = y - b->py;

		if ( !Touch_Visible( i ) ) {
			continue;
		}
		if ( b->pr > 0.0f ) {
			// Generous: thumbs land a little off
			float d = sqrt( dx * dx + dy * dy ) / ( b->pr * 1.2f );

			if ( d <= 1.0f && ( best < 0 || d < bestDist ) ) {
				best = i;
				bestDist = d;
			}
		} else if ( fabs( dx ) <= b->phw * 1.2f && fabs( dy ) <= b->phh ) {
			return i;
		}
	}

	return best;
}

/*
=================
Touch_ButtonDown / Touch_ButtonUp
=================
*/
static void Touch_ButtonDown( int button, int finger ) {
	touchButton_t *b = &touchButtons[button];

	b->finger = finger;
	b->downTime = cls.realtime;

	switch ( button ) {
	case TB_PAUSE:
		Com_QueueEvent( 0, SE_KEY, K_ESCAPE, qtrue, 0, NULL );
		Com_QueueEvent( 0, SE_KEY, K_ESCAPE, qfalse, 0, NULL );
		break;
	case TB_AURA:
		tc.auraMode = AURA_WAIT;
		break;
	case TB_SKILL:
		tc.skillDrag = 0.0f;
		tc.skillStepped = qfalse;
		break;
	case TB_LOCK:
		tc.lockPressed = qtrue;
		break;
	default:
		break;
	}
}

static void Touch_ButtonUp( int button ) {
	touchButton_t *b = &touchButtons[button];

	// A tap shorter than one frame still reaches one usercmd
	if ( b->bits || button == TB_FLYUP ) {
		b->latched = qtrue;
	}

	// The guided combo ran: letting go must not also break
	if ( button == TB_KI && tc.ctx == CTX_MELEE && touch_profile && touch_profile->integer &&
		cls.realtime - b->downTime >= COMBO_HOLD_MS ) {
		b->latched = qfalse;
	}

	switch ( button ) {
	case TB_SKILL:
		if ( !tc.skillStepped ) {
			Cbuf_AddText( "weapnext\n" );
		}
		break;
	case TB_AURA:
		tc.auraMode = AURA_NONE;
		b->latched = qfalse;
		break;
	default:
		break;
	}

	b->finger = -1;
}

/*
=================
Touch_ReleaseFinger
=================
*/
static void Touch_ReleaseFinger( touchFinger_t *f ) {
	int index = f - tc.fingers;

	switch ( f->kind ) {
	case TF_STICK:
		if ( tc.stickFinger == index ) {
			tc.stickFinger = -1;
			tc.boost = qfalse;
			tc.boostOverSince = 0;
		}
		break;
	case TF_BUTTON:
		if ( touchButtons[f->button].finger == index ) {
			Touch_ButtonUp( f->button );
		}
		break;
	case TF_MENU:
		Com_QueueEvent( 0, SE_KEY, K_MOUSE1, qfalse, 0, NULL );
		break;
	default:
		break;
	}

	f->active = qfalse;
	f->kind = TF_NONE;
}

/*
=================
CL_TouchReset

Lets go of everything: the app lost focus, or a menu took over the screen.
=================
*/
void CL_TouchReset( void ) {
	int i;

	for ( i = 0; i < TOUCH_MAX_FINGERS; i++ ) {
		if ( tc.fingers[i].active ) {
			Touch_ReleaseFinger( &tc.fingers[i] );
		}
	}
	for ( i = 0; i < TB_COUNT; i++ ) {
		touchButtons[i].finger = -1;
		touchButtons[i].latched = qfalse;
	}
	tc.stickFinger = -1;
	tc.boost = qfalse;
	tc.lookX = tc.lookY = 0.0f;
	tc.auraMode = AURA_NONE;
}

/*
=================
Touch_MenuCursor

The UI only takes relative mouse moves. Pushing far past the corner first
pins its cursor there, since UI_MouseEvent clamps it, so the second move
lands exactly under the finger. The math mirrors UI_Init and UI_MouseEvent.
=================
*/
static void Touch_MenuCursor( float x, float y ) {
	float scale = tc.h / 480.0f;
	float bias = 0.0f;
	float vx, vy, minX;

	if ( tc.w * 480.0f > tc.h * 640.0f ) {
		bias = 0.5f * ( tc.w - tc.h * ( 640.0f / 480.0f ) );
	}
	vx = ( x - bias ) / scale;
	vy = y / scale;
	minX = -bias / scale;

	Com_QueueEvent( 0, SE_MOUSE, -100000, -100000, 0, NULL );
	Com_QueueEvent( 0, SE_MOUSE, (int)( vx - minX + 0.5f ), (int)( vy + 0.5f ), 0, NULL );
}

/*
=================
Touch_GameActive
=================
*/
static qboolean Touch_GameActive( void ) {
	return touch_enable->integer && cls.state == CA_ACTIVE &&
		!( Key_GetCatcher() & ( KEYCATCH_UI | KEYCATCH_CONSOLE | KEYCATCH_MESSAGE | KEYCATCH_CGAME ) );
}

/*
=================
Touch_FingerDown
=================
*/
static void Touch_FingerDown( touchFinger_t *f ) {
	int index = f - tc.fingers;
	int button;

	if ( Key_GetCatcher() & KEYCATCH_UI ) {
		// Menus: only one finger drives the cursor
		int i;

		for ( i = 0; i < TOUCH_MAX_FINGERS; i++ ) {
			if ( i != index && tc.fingers[i].active && tc.fingers[i].kind == TF_MENU ) {
				return;
			}
		}
		f->kind = TF_MENU;
		Touch_MenuCursor( f->x, f->y );
		Com_QueueEvent( 0, SE_KEY, K_MOUSE1, qtrue, 0, NULL );
		return;
	}

	if ( !Touch_GameActive() ) {
		return;
	}

	button = Touch_HitButton( f->x, f->y );
	if ( button >= 0 && touchButtons[button].finger < 0 ) {
		f->kind = TF_BUTTON;
		f->button = button;
		Touch_ButtonDown( button, index );
		return;
	}

	if ( f->x < tc.w * 0.5f ) {
		if ( tc.stickFinger < 0 ) {
			float margin = ( EDGE_MARGIN + STICK_RADIUS ) * tc.dp;

			f->kind = TF_STICK;
			tc.stickFinger = index;
			tc.stickX = Com_Clamp( margin, tc.w * 0.5f, f->x );
			tc.stickY = Com_Clamp( margin, tc.h - margin, f->y );
			tc.boost = qfalse;
			tc.boostOverSince = 0;
		}
		return;
	}

	f->kind = TF_LOOK;
}

/*
=================
Touch_FingerMove
=================
*/
static void Touch_FingerMove( touchFinger_t *f ) {
	float dx = ( f->x - f->lastX ) / tc.dp;
	float dy = ( f->y - f->lastY ) / tc.dp;

	switch ( f->kind ) {
	case TF_MENU:
		Touch_MenuCursor( f->x, f->y );
		break;

	case TF_LOOK:
		tc.lookX += dx;
		tc.lookY += dy;
		break;

	case TF_BUTTON: {
		touchButton_t *b = &touchButtons[f->button];

		if ( b->dragLook ) {
			if ( !f->looking ) {
				float ox = f->x - b->px;
				float oy = f->y - b->py;

				// Charge with the thumb, then slide off to aim: the button stays held
				f->looking = sqrt( ox * ox + oy * oy ) > b->pr * 1.3f;
			}
			if ( f->looking ) {
				tc.lookX += dx;
				tc.lookY += dy;
			}
		} else if ( f->button == TB_AURA && tc.auraMode == AURA_WAIT ) {
			float swipe = ( f->y - f->y0 ) / tc.dp;

			if ( swipe < -AURA_SWIPE ) {
				tc.auraMode = AURA_TIER_UP;
			} else if ( swipe > AURA_SWIPE ) {
				tc.auraMode = AURA_TIER_DOWN;
			}
		} else if ( f->button == TB_SKILL ) {
			// Slide up or down on TECNICA to step through the techniques
			tc.skillDrag += dy;
			while ( tc.skillDrag <= -SKILL_STEP ) {
				Cbuf_AddText( "weapprev\n" );
				tc.skillDrag += SKILL_STEP;
				tc.skillStepped = qtrue;
			}
			while ( tc.skillDrag >= SKILL_STEP ) {
				Cbuf_AddText( "weapnext\n" );
				tc.skillDrag -= SKILL_STEP;
				tc.skillStepped = qtrue;
			}
		}
		break;
	}

	default:
		break;
	}
}

/*
=================
CL_TouchEvent

From the SDL event loop. x and y are 0..1 across the window.
=================
*/
void CL_TouchEvent( touchEventType_t type, long long id, float x, float y ) {
	touchFinger_t *f = NULL;
	int i;

	if ( !touch_enable ) {
		return;
	}
	Touch_Layout();

	for ( i = 0; i < TOUCH_MAX_FINGERS; i++ ) {
		if ( tc.fingers[i].active && tc.fingers[i].id == id ) {
			f = &tc.fingers[i];
			break;
		}
	}

	switch ( type ) {
	case TOUCH_DOWN:
		if ( f ) {
			Touch_ReleaseFinger( f );
		}
		for ( i = 0; i < TOUCH_MAX_FINGERS && tc.fingers[i].active; i++ ) {
		}
		if ( i == TOUCH_MAX_FINGERS ) {
			return;
		}
		f = &tc.fingers[i];
		Com_Memset( f, 0, sizeof( *f ) );
		f->active = qtrue;
		f->id = id;
		f->x = f->x0 = f->lastX = x * tc.w;
		f->y = f->y0 = f->lastY = y * tc.h;
		f->downTime = cls.realtime;
		Touch_FingerDown( f );
		break;

	case TOUCH_MOVE:
		if ( !f ) {
			return;
		}
		f->lastX = f->x;
		f->lastY = f->y;
		f->x = x * tc.w;
		f->y = y * tc.h;
		Touch_FingerMove( f );
		break;

	case TOUCH_UP:
		if ( f ) {
			f->x = x * tc.w;
			f->y = y * tc.h;
			Touch_ReleaseFinger( f );
		}
		break;
	}
}

/*
=================
Touch_NearGround
=================
*/
static qboolean Touch_NearGround( void ) {
	trace_t	trace;
	vec3_t	start, end;
	vec3_t	mins = { -15, -15, -24 };
	vec3_t	maxs = { 15, 15, 32 };

	VectorCopy( cl.snap.ps.origin, start );
	VectorCopy( start, end );
	end[2] -= LAND_TRACE;
	CM_BoxTrace( &trace, start, end, mins, maxs, 0, CONTENTS_SOLID, qfalse );

	return trace.fraction < 1.0f;
}

/*
=================
Touch_StickMove
=================
*/
static void Touch_StickMove( usercmd_t *cmd ) {
	touchFinger_t *f = &tc.fingers[tc.stickFinger];
	float radius = STICK_RADIUS * tc.dp;
	float sx = ( f->x - tc.stickX ) / radius;
	float sy = ( tc.stickY - f->y ) / radius;	// screen y grows downward
	float mag = sqrt( sx * sx + sy * sy );
	float speed, nx, ny;

	// Boost: past 120 % for 120 ms, until back under 100 %
	if ( mag > STICK_BOOST ) {
		if ( !tc.boostOverSince ) {
			tc.boostOverSince = cls.realtime;
		} else if ( cls.realtime - tc.boostOverSince >= STICK_BOOST_MS ) {
			tc.boost = qtrue;
		}
	} else {
		tc.boostOverSince = 0;
		if ( mag < 1.0f ) {
			tc.boost = qfalse;
		}
	}

	tc.stickMag = mag;
	if ( mag < STICK_DEAD ) {
		return;
	}

	nx = sx / mag;
	ny = sy / mag;
	tc.stickNX = nx;
	tc.stickNY = ny;

	// With lock-on, melee and the F chords read only the sign of each axis.
	// A thumb never moves straight: drop the minor axis unless it is clear.
	if ( cl.snap.valid && cl.snap.ps.lockedTarget > 0 ) {
		if ( fabs( nx ) < STICK_SECTOR * fabs( ny ) ) {
			nx = 0.0f;
			ny = ny > 0.0f ? 1.0f : -1.0f;
		} else if ( fabs( ny ) < STICK_SECTOR * fabs( nx ) ) {
			ny = 0.0f;
			nx = nx > 0.0f ? 1.0f : -1.0f;
		}
	}

	if ( mag < STICK_WALK ) {
		// Walking: PmoveSingle drops BUTTON_WALKING above 64/127
		speed = 64.0f * ( mag - STICK_DEAD ) / ( STICK_WALK - STICK_DEAD );
		cmd->buttons |= BUTTON_WALKING;
	} else {
		speed = 64.0f + 63.0f * Com_Clamp( 0.0f, 1.0f, ( mag - STICK_WALK ) / ( 1.0f - STICK_WALK ) );
		cmd->buttons &= ~BUTTON_WALKING;
	}

	cmd->forwardmove = ClampChar( (int)( ny * speed ) );
	cmd->rightmove = ClampChar( (int)( nx * speed ) );

	if ( tc.boost ) {
		cmd->buttons |= BUTTON_BOOST;
	}
}

/*
=================
CL_TouchMove

Called by CL_CreateCmd after the keyboard, the mouse and the joystick.
=================
*/
void CL_TouchMove( usercmd_t *cmd ) {
	touchButton_t *b;
	float lookYaw = 0.0f, lookPitch = 0.0f;
	qboolean assisted;
	int i;

	if ( !touch_enable ) {
		return;
	}
	Touch_UpdateContext();
	assisted = touch_profile && touch_profile->integer;

	if ( !Touch_GameActive() ) {
		// A menu or the console took over: the game must not see held buttons
		if ( cls.state == CA_ACTIVE ) {
			for ( i = 0; i < TOUCH_MAX_FINGERS; i++ ) {
				touchFinger_t *f = &tc.fingers[i];

				if ( f->active && f->kind != TF_MENU && f->kind != TF_NONE ) {
					Touch_ReleaseFinger( f );
					f->active = qtrue;	// keep it until it lifts, doing nothing
				}
			}
			for ( i = 0; i < TB_COUNT; i++ ) {
				touchButtons[i].latched = qfalse;
			}
			tc.lookX = tc.lookY = 0.0f;
		}
		return;
	}

	Touch_Layout();

	// Camera: drag right to turn right, drag up to look up. The aim assist
	// slows it down near an opponent (friction).
	if ( tc.lookX || tc.lookY ) {
		float scale = touch_lookSpeed->value * CL_AimAssistLookScale();

		lookYaw = -tc.lookX * scale;
		lookPitch = tc.lookY * scale;
		cl.viewangles[YAW] += lookYaw;
		cl.viewangles[PITCH] += lookPitch;
		tc.lookX = tc.lookY = 0.0f;
	}

	tc.stickMag = 0.0f;
	if ( tc.stickFinger >= 0 ) {
		Touch_StickMove( cmd );
	}

	for ( i = 0; i < TB_COUNT; i++ ) {
		b = &touchButtons[i];
		if ( i == TB_KI || !Touch_Visible( i ) ) {
			continue;
		}
		if ( b->bits && ( b->finger >= 0 || b->latched ) ) {
			cmd->buttons |= b->bits;
		}
	}

	// KI: in melee with the Asistido profile, holding it plays the guided
	// combo (rush with W, then FUERTE past 550 ms for the power launch);
	// a tap still breaks. Everywhere else it is MOUSE1.
	b = &touchButtons[TB_KI];
	if ( tc.ctx != CTX_STRUGGLE ) {
		if ( b->finger >= 0 ) {
			int held = cls.realtime - b->downTime;

			if ( tc.ctx == CTX_MELEE && assisted ) {
				if ( held >= COMBO_HOLD_MS ) {
					int phase = ( held - COMBO_HOLD_MS ) % COMBO_CYCLE_MS;

					if ( phase < COMBO_RUSH_MS ) {
						cmd->forwardmove = 127;
					} else if ( phase < COMBO_RUSH_MS + COMBO_LAUNCH_MS ) {
						cmd->buttons |= BUTTON_ALT_ATTACK;
					}
				}
			} else {
				cmd->buttons |= BUTTON_ATTACK;
			}
		} else if ( b->latched ) {
			cmd->buttons |= BUTTON_ATTACK;
		}
	}

	// FUERTE held in melee: the stick picks where the power launch sends it,
	// the R, C, D and A of PC (PM_Melee reads upmove, then rightmove)
	if ( tc.ctx == CTX_MELEE && touchButtons[TB_STRONG].finger >= 0 && tc.stickMag >= STICK_DEAD ) {
		cmd->forwardmove = 0;
		cmd->rightmove = 0;
		if ( fabs( tc.stickNY ) >= fabs( tc.stickNX ) ) {
			cmd->upmove = tc.stickNY > 0.0f ? 127 : -127;
		} else {
			cmd->rightmove = tc.stickNX > 0.0f ? 127 : -127;
		}
	}

	// LOCK: face the opponent nearest to the view first, so the lock-on of
	// the game picks that one. The game traces the lock-on before it applies
	// the angles of the command (PM_CheckLockon runs before
	// PM_UpdateViewAngles), so the button goes one command after the turn:
	// up in this one, down in the next, even if the finger already left.
	if ( tc.lockSendNext ) {
		cmd->buttons |= BUTTON_GESTURE;
		tc.lockSendNext = qfalse;
	}
	if ( tc.lockPressed ) {
		if ( cl.snap.valid && cl.snap.ps.lockedTarget <= 0 && CL_AimAssistFaceTarget() ) {
			cmd->buttons &= ~BUTTON_GESTURE;
			tc.lockSendNext = qtrue;
		}
		tc.lockPressed = qfalse;
	}

	// VUELO up: a tap jumps, holding flies up
	b = &touchButtons[TB_FLYUP];
	if ( b->finger >= 0 ) {
		if ( cls.realtime - b->downTime < JUMP_TAP_MS ) {
			cmd->buttons |= BUTTON_JUMP;
		} else {
			cmd->upmove = 127;
		}
	} else if ( b->latched ) {
		cmd->buttons |= BUTTON_JUMP;
	}

	// VUELO down: descend; close to the ground also lands (F + C on PC)
	b = &touchButtons[TB_FLYDOWN];
	if ( b->finger >= 0 ) {
		cmd->upmove = -127;
		if ( cl.snap.valid && ( cl.snap.ps.bitFlags & usingFlight ) && Touch_NearGround() ) {
			cmd->buttons |= BUTTON_POWERLEVEL;
		}
	}

	// AURA: the F chords. Hold to charge (F + D); with the stick pushed forward
	// it transforms (F + W) and pulled back it reverts (F + S); a swipe up or
	// down on AURA does the same without the stick
	b = &touchButtons[TB_AURA];
	if ( b->finger >= 0 ) {
		int stick = 0;

		if ( tc.stickFinger >= 0 && tc.stickMag >= STICK_WALK && fabs( tc.stickNY ) >= fabs( tc.stickNX ) ) {
			stick = tc.stickNY > 0.0f ? 1 : -1;
		}
		if ( tc.auraMode == AURA_WAIT && cls.realtime - b->downTime >= AURA_CHARGE_MS ) {
			tc.auraMode = AURA_CHARGE;
		}
		cmd->buttons |= BUTTON_POWERLEVEL;
		cmd->forwardmove = 0;
		cmd->rightmove = 0;
		if ( stick ) {
			cmd->forwardmove = stick > 0 ? 127 : -127;
		} else {
			switch ( tc.auraMode ) {
			case AURA_CHARGE:		cmd->rightmove = 127; break;
			case AURA_TIER_UP:		cmd->forwardmove = 127; break;
			case AURA_TIER_DOWN:	cmd->forwardmove = -127; break;
			default:				break;
			}
		}
	}

	for ( i = 0; i < TB_COUNT; i++ ) {
		touchButtons[i].latched = qfalse;
	}

	CL_AimAssist( cmd, lookYaw, lookPitch );
}

/*
=================
Touch_DrawPic
=================
*/
static void Touch_DrawPic( float cx, float cy, float w, float h, const float *rgb, float alpha, qhandle_t shader ) {
	vec4_t color;

	if ( !shader ) {
		return;
	}
	color[0] = rgb ? rgb[0] : 1.0f;
	color[1] = rgb ? rgb[1] : 1.0f;
	color[2] = rgb ? rgb[2] : 1.0f;
	color[3] = alpha;
	re.SetColor( color );
	re.DrawStretchPic( cx - w * 0.5f, cy - h * 0.5f, w, h, 0, 0, 1, 1, shader );
}

/*
=================
CL_TouchRegisterMedia

Called by CL_InitRenderer, also after vid_restart.
=================
*/
void CL_TouchRegisterMedia( void ) {
	int i;

	tc.disc = re.RegisterShaderNoMip( "touch/disc" );
	tc.ring = re.RegisterShaderNoMip( "touch/ring" );
	tc.glow = re.RegisterShaderNoMip( "touch/glow" );
	tc.knob = re.RegisterShaderNoMip( "touch/knob" );
	tc.stickBase = re.RegisterShaderNoMip( "touch/stickbase" );
	tc.pill = re.RegisterShaderNoMip( "touch/pill" );
	tc.arrowUp = re.RegisterShaderNoMip( "touch/arrow_up" );
	tc.arrowDown = re.RegisterShaderNoMip( "touch/arrow_down" );
	tc.pause = re.RegisterShaderNoMip( "touch/pause" );

	for ( i = 0; i < TL_COUNT; i++ ) {
		tc.labels[i] = touchLabelImages[i] ?
			re.RegisterShaderNoMip( va( "touch/label_%s", touchLabelImages[i] ) ) : 0;
	}
	tc.mediaRegistered = qtrue;
}

/*
=================
CL_TouchDraw

Called by SCR_DrawScreenField over the game view, under menus and console.
=================
*/
void CL_TouchDraw( void ) {
	float alpha;
	int i;

	if ( !touch_enable || !Touch_GameActive() || !tc.mediaRegistered ) {
		return;
	}
	Touch_Layout();
	Touch_UpdateContext();
	alpha = Com_Clamp( 0.1f, 1.0f, touch_alpha->value );

	// Stick: the base where the thumb landed, or a faint hint of where it goes
	if ( tc.stickFinger >= 0 ) {
		touchFinger_t *f = &tc.fingers[tc.stickFinger];
		float radius = STICK_RADIUS * tc.dp;
		float kx = f->x - tc.stickX;
		float ky = f->y - tc.stickY;
		float d = sqrt( kx * kx + ky * ky );
		float maxd = radius * STICK_BOOST;

		if ( d > maxd ) {
			kx *= maxd / d;
			ky *= maxd / d;
		}
		Touch_DrawPic( tc.stickX, tc.stickY, radius * 2, radius * 2, NULL, alpha, tc.stickBase );
		if ( tc.boost ) {
			Touch_DrawPic( tc.stickX, tc.stickY, radius * 2.6f, radius * 2.6f, touchButtons[TB_KI].tint, alpha * 0.6f, tc.glow );
		}
		Touch_DrawPic( tc.stickX + kx, tc.stickY + ky, STICK_KNOB * 2 * tc.dp, STICK_KNOB * 2 * tc.dp, NULL, alpha, tc.knob );
	} else {
		float r = STICK_RADIUS * tc.dp;

		Touch_DrawPic( STICK_IDLE_X * tc.dp, tc.h - STICK_IDLE_Y * tc.dp, r * 2, r * 2, NULL, alpha * 0.35f, tc.stickBase );
	}

	// VUELO: one capsule, two halves
	if ( Touch_Visible( TB_FLYUP ) ) {
		touchButton_t *up = &touchButtons[TB_FLYUP];
		touchButton_t *down = &touchButtons[TB_FLYDOWN];
		float cx = up->px;
		float cy = ( up->py + down->py ) * 0.5f;
		float w = up->phw * 2;
		float h = up->phh * 4;
		float arrow = w * 0.62f;

		Touch_DrawPic( cx, cy, w, h, NULL, alpha, tc.pill );
		if ( up->finger >= 0 ) {
			Touch_DrawPic( up->px, up->py, w * 1.6f, w * 1.6f, NULL, alpha * 0.6f, tc.glow );
		}
		if ( down->finger >= 0 ) {
			Touch_DrawPic( down->px, down->py, w * 1.6f, w * 1.6f, NULL, alpha * 0.6f, tc.glow );
		}
		Touch_DrawPic( up->px, up->py, arrow, arrow, NULL, alpha, tc.arrowUp );
		Touch_DrawPic( down->px, down->py, arrow, arrow, NULL, alpha, tc.arrowDown );
	}

	for ( i = 0; i < TB_COUNT; i++ ) {
		touchButton_t *b = &touchButtons[i];
		qboolean held = b->finger >= 0;
		qboolean dim;
		touchLabel_t label;
		float size, a;

		if ( b->pr <= 0.0f || !Touch_Visible( i ) ) {
			continue;
		}
		size = b->pr * 2;
		label = Touch_Label( i, &dim );
		a = dim ? alpha * 0.4f : alpha;

		// Z glows while its window is open (counter, dodge, chase)
		if ( i == TB_Z && tc.zHint && !held ) {
			float pulse = 0.55f + 0.45f * sin( cls.realtime * 0.012f );

			Touch_DrawPic( b->px, b->py, size * 1.7f, size * 1.7f, b->tint, alpha * pulse, tc.glow );
		}
		if ( held ) {
			Touch_DrawPic( b->px, b->py, size * 1.5f, size * 1.5f, b->tint, a * 0.7f, tc.glow );
		}
		Touch_DrawPic( b->px, b->py, size, size, NULL, a * ( held ? 1.0f : 0.85f ), tc.disc );
		Touch_DrawPic( b->px, b->py, size, size, b->tint, a * ( held ? 1.0f : 0.8f ), tc.ring );

		if ( i == TB_PAUSE ) {
			Touch_DrawPic( b->px, b->py, size * 0.7f, size * 0.7f, NULL, a, tc.pause );
		} else if ( label != TL_NONE ) {
			// Labels are 2:1 images; short ones fill more of the button
			float lw = size * 0.92f;

			Touch_DrawPic( b->px, b->py, lw, lw * 0.5f, NULL, a * 1.1f, tc.labels[label] );
		}
	}

	if ( touch_debug->integer ) {
		for ( i = 0; i < TOUCH_MAX_FINGERS; i++ ) {
			if ( tc.fingers[i].active ) {
				Touch_DrawPic( tc.fingers[i].x, tc.fingers[i].y, 30 * tc.dp, 30 * tc.dp, NULL, 0.8f, tc.knob );
			}
		}
	}

	re.SetColor( NULL );
}

/*
=================
CL_TouchInit
=================
*/
void CL_TouchInit( void ) {
	int i;

	touch_enable = Cvar_Get( "touch_enable", "1", CVAR_ARCHIVE );
	touch_alpha = Cvar_Get( "touch_alpha", "0.7", CVAR_ARCHIVE );
	touch_scale = Cvar_Get( "touch_scale", "1", CVAR_ARCHIVE );
	touch_lookSpeed = Cvar_Get( "touch_lookSpeed", "0.3", CVAR_ARCHIVE );
	touch_debug = Cvar_Get( "touch_debug", "0", 0 );

	CL_AimAssistInit();
	touch_profile = Cvar_Get( "touch_profile", "1", CVAR_ARCHIVE );

	Com_Memset( &tc, 0, sizeof( tc ) );
	tc.stickFinger = -1;
	for ( i = 0; i < TB_COUNT; i++ ) {
		touchButtons[i].finger = -1;
		touchButtons[i].latched = qfalse;
	}
}

#endif // USE_TOUCH
