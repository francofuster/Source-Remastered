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

Contexts, macros, auto lock and aim assist belong to phase F3.
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
	TB_COUNT
} touchButtonId_t;

typedef struct {
	const char	*image;		// touch/label_<image>, NULL for none
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
	qhandle_t	label;
	int			finger;		// index in tc.fingers, -1 when up
	int			downTime;
	qboolean	latched;	// pressed and released before the next usercmd
} touchButton_t;

static touchButton_t touchButtons[TB_COUNT] = {
	//            image     bits                 x      y      r    hw    hh   top     cx     look
	/* KI      */ { "ki",     BUTTON_ATTACK,      92,    92,   44,   0,    0,  qfalse, qfalse, qtrue,  { 1.00f, 0.62f, 0.15f } },
	/* FUERTE  */ { "strong", BUTTON_ALT_ATTACK, 188,    72,   36,   0,    0,  qfalse, qfalse, qtrue,  { 1.00f, 0.32f, 0.26f } },
	/* Z       */ { "z",      BUTTON_TELEPORT,   162,   164,   36,   0,    0,  qfalse, qfalse, qfalse, { 0.30f, 0.86f, 1.00f } },
	/* GUARDIA */ { "guard",  BUTTON_BLOCK,       68,   192,   34,   0,    0,  qfalse, qfalse, qfalse, { 0.45f, 0.58f, 1.00f } },
	/* LOCK    */ { "lock",   BUTTON_GESTURE,     58,   108,   28,   0,    0,  qtrue,  qfalse, qfalse, { 1.00f, 0.86f, 0.30f } },
	/* AURA    */ { "aura",   BUTTON_POWERLEVEL, 268,   194,   28,   0,    0,  qfalse, qfalse, qfalse, { 0.75f, 0.85f, 1.00f } },
	/* TECNICA */ { "skill",  0,                 160,   254,   27,   0,    0,  qfalse, qfalse, qfalse, { 0.45f, 0.95f, 0.55f } },
	/* VUELO + */ { NULL,     0,                 268,   122,    0,  27,   28,  qfalse, qfalse, qfalse, { 1.00f, 1.00f, 1.00f } },
	/* VUELO - */ { NULL,     0,                 268,    66,    0,  27,   28,  qfalse, qfalse, qfalse, { 1.00f, 1.00f, 1.00f } },
	/* pause   */ { NULL,     0,                   0,    32,   20,   0,    0,  qtrue,  qtrue,  qfalse, { 1.00f, 1.00f, 1.00f } },
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

	qhandle_t		disc, ring, glow, knob, stickBase, pill, arrowUp, arrowDown, pause;
} tc;

enum { AURA_NONE, AURA_WAIT, AURA_CHARGE, AURA_TIER_UP, AURA_TIER_DOWN };

static cvar_t	*touch_enable;
static cvar_t	*touch_alpha;
static cvar_t	*touch_scale;
static cvar_t	*touch_lookSpeed;
static cvar_t	*touch_debug;

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

	if ( mag < STICK_DEAD ) {
		return;
	}

	nx = sx / mag;
	ny = sy / mag;

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
	int i;

	if ( !touch_enable ) {
		return;
	}

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

	// Camera: drag right to turn right, drag up to look up
	if ( tc.lookX || tc.lookY ) {
		cl.viewangles[YAW] -= tc.lookX * touch_lookSpeed->value;
		cl.viewangles[PITCH] += tc.lookY * touch_lookSpeed->value;
		tc.lookX = tc.lookY = 0.0f;
	}

	if ( tc.stickFinger >= 0 ) {
		Touch_StickMove( cmd );
	}

	for ( i = 0; i < TB_COUNT; i++ ) {
		b = &touchButtons[i];
		if ( b->bits && ( b->finger >= 0 || b->latched ) ) {
			cmd->buttons |= b->bits;
		}
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

	// AURA: the F chords. Hold to charge (F + D), swipe up or down to change tier (F + W, F + S)
	b = &touchButtons[TB_AURA];
	if ( b->finger >= 0 ) {
		if ( tc.auraMode == AURA_WAIT && cls.realtime - b->downTime >= AURA_CHARGE_MS ) {
			tc.auraMode = AURA_CHARGE;
		}
		cmd->buttons |= BUTTON_POWERLEVEL;
		cmd->forwardmove = 0;
		cmd->rightmove = 0;
		switch ( tc.auraMode ) {
		case AURA_CHARGE:		cmd->rightmove = 127; break;
		case AURA_TIER_UP:		cmd->forwardmove = 127; break;
		case AURA_TIER_DOWN:	cmd->forwardmove = -127; break;
		default:				break;
		}
	}

	for ( i = 0; i < TB_COUNT; i++ ) {
		touchButtons[i].latched = qfalse;
	}
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

	for ( i = 0; i < TB_COUNT; i++ ) {
		touchButtons[i].label = touchButtons[i].image ?
			re.RegisterShaderNoMip( va( "touch/label_%s", touchButtons[i].image ) ) : 0;
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
	{
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
		float size;

		if ( b->pr <= 0.0f ) {
			continue;
		}
		size = b->pr * 2;

		if ( held ) {
			Touch_DrawPic( b->px, b->py, size * 1.5f, size * 1.5f, b->tint, alpha * 0.7f, tc.glow );
		}
		Touch_DrawPic( b->px, b->py, size, size, NULL, alpha * ( held ? 1.0f : 0.85f ), tc.disc );
		Touch_DrawPic( b->px, b->py, size, size, b->tint, alpha * ( held ? 1.0f : 0.8f ), tc.ring );

		if ( i == TB_PAUSE ) {
			Touch_DrawPic( b->px, b->py, size * 0.7f, size * 0.7f, NULL, alpha, tc.pause );
		} else if ( b->label ) {
			// Labels are 2:1 images; short ones fill more of the button
			float lw = size * 0.92f;

			Touch_DrawPic( b->px, b->py, lw, lw * 0.5f, NULL, alpha * 1.1f, b->label );
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

	Com_Memset( &tc, 0, sizeof( tc ) );
	tc.stickFinger = -1;
	for ( i = 0; i < TB_COUNT; i++ ) {
		touchButtons[i].finger = -1;
		touchButtons[i].latched = qfalse;
	}
}

#endif // USE_TOUCH
