/*
===========================================================================
cl_aimassist.c -- aim assist for the touch controls (Android, USE_TOUCH)

Works in the engine client, on cl.viewangles and the entities of the last
snapshot, so it needs no change in the game modules and works with any
server. A server can forbid it with sv_aimAssist 0 (it travels in the
systeminfo).

Zones are angles from the view direction, in degrees:
  capture  (cl_aaCapture)   LOCK picks the opponent with the smallest angle
  friction (cl_aaFriction)  touch look slows down to cl_aaSens near it
  magnet   (cl_aaMagnet)    the view turns toward it (cl_aaPull deg/s) and
                            follows its motion (cl_aaTrack)
  core     its own angular size + 1.2: the magnet stops there; a shot is
           bent up to cl_aaBend degrees onto it

Only without lock-on: with lock-on PM_UpdateViewAngles already aims.
Values and rules: the Android proposal, section "Ayuda de punteria".
===========================================================================
*/

#ifdef USE_TOUCH

#include "client.h"

#define AA_TARGET_HEIGHT	24.0f	// aim at the chest, not the feet
#define AA_TARGET_RADIUS	28.0f	// for the angular size of the core
#define AA_CORE_MARGIN		1.2f
#define AA_KEEP				1.25f	// a target is kept up to this times the friction radius
#define AA_SWITCH_ANGLE		3.0f	// another one must be this much better...
#define AA_SWITCH_MS		200		// ...for this long
#define AA_BREAK_SPEED		120.0f	// deg/s of touch look away from it releases the magnet...
#define AA_BREAK_MS			350		// ...for this long

static cvar_t	*cl_aimAssist;
static cvar_t	*cl_aaCapture;
static cvar_t	*cl_aaFriction;
static cvar_t	*cl_aaSens;
static cvar_t	*cl_aaMagnet;
static cvar_t	*cl_aaPull;
static cvar_t	*cl_aaPassive;
static cvar_t	*cl_aaTrack;
static cvar_t	*cl_aaBend;
static cvar_t	*cl_aaDwell;
static cvar_t	*cl_aaDebug;
static cvar_t	*touch_profile;

typedef struct {
	int		num;			// entity number, -1 for none
	vec3_t	point;			// where to aim
	float	yaw, pitch;		// view angles that face it
	float	theta;			// angle from the view direction
	float	core;
} aaTarget_t;

static struct {
	int		target;				// entity followed by the magnet, -1 for none
	int		challenger;
	int		challengerSince;
	float	lastYaw, lastPitch;	// target angles last frame, for the tracking
	int		lastTarget;			// whose angles those are, -1 for none
	int		lastTime;
	int		breakUntil;
	int		dwell;				// ms attacking with the target in the magnet
	qboolean	dwellFired;
	int		lastButtons;
	float	lookScale;
	qboolean	verbose;		// cl_aaDebug while LOCK looks for a target
} aa;

/*
=================
AA_Allowed
=================
*/
static qboolean AA_Allowed( void ) {
	const char *systemInfo;

	if ( !cl_aimAssist->integer || cls.state != CA_ACTIVE || !cl.snap.valid ) {
		return qfalse;
	}
	if ( Key_GetCatcher() & ( KEYCATCH_UI | KEYCATCH_CONSOLE | KEYCATCH_MESSAGE ) ) {
		return qfalse;
	}
	// with lock-on the game aims by itself
	if ( cl.snap.ps.lockedTarget > 0 ) {
		return qfalse;
	}
	// the server may forbid it, for matches with PC players
	systemInfo = cl.gameState.stringData + cl.gameState.stringOffsets[ CS_SYSTEMINFO ];
	if ( !strcmp( Info_ValueForKey( systemInfo, "sv_aimAssist" ), "0" ) ) {
		return qfalse;
	}
	return qtrue;
}

/*
=================
AA_Eye
=================
*/
static void AA_Eye( vec3_t eye ) {
	VectorCopy( cl.snap.ps.origin, eye );
	eye[2] += cl.snap.ps.viewheight;
}

/*
=================
AA_View

Where the player really looks: the game adds delta_angles to the angles of
the command (spawn, setviewpos and the lock-on view move them), so
cl.viewangles alone can be far off. Turning is still done on cl.viewangles:
adding to it turns the real view by the same amount.
=================
*/
static float AA_View( int axis ) {
	return cl.viewangles[axis] + SHORT2ANGLE( cl.snap.ps.delta_angles[axis] );
}

/*
=================
AA_Measure

Fills the angles of an entity of the snapshot; qfalse when it cannot be a
target (not a player, us, dead, a teammate, hidden behind the map).
=================
*/
static qboolean AA_Measure( entityState_t *es, aaTarget_t *t ) {
	vec3_t	eye, dir, forward, angles;
	vec3_t	zero = { 0, 0, 0 };
	trace_t	trace;
	float	dist, dt, dot;

	if ( es->eType != ET_PLAYER || es->number == cl.snap.ps.clientNum || es->number >= MAX_CLIENTS ) {
		return qfalse;
	}
	if ( es->eFlags & EF_DEAD ) {
		return qfalse;
	}
	if ( cl.snap.ps.persistant[PERS_TEAM] != TEAM_FREE ) {
		const char *info = cl.gameState.stringData + cl.gameState.stringOffsets[ CS_PLAYERS + es->number ];

		if ( atoi( Info_ValueForKey( info, "t" ) ) == cl.snap.ps.persistant[PERS_TEAM] ) {
			return qfalse;
		}
	}

	// Snapshots come 20 times a second: move it on to now
	dt = ( cl.serverTime - cl.snap.serverTime ) * 0.001f;
	if ( dt < 0.0f ) {
		dt = 0.0f;
	} else if ( dt > 0.1f ) {
		dt = 0.1f;
	}
	VectorMA( es->pos.trBase, dt, es->pos.trDelta, t->point );
	t->point[2] += AA_TARGET_HEIGHT;

	AA_Eye( eye );
	VectorSubtract( t->point, eye, dir );
	dist = VectorNormalize( dir );
	if ( dist < 1.0f ) {
		return qfalse;
	}

	vectoangles( dir, angles );
	t->yaw = angles[YAW];
	t->pitch = angles[PITCH];

	{
		vec3_t view;
		view[PITCH] = AA_View( PITCH );
		view[YAW] = AA_View( YAW );
		view[ROLL] = 0.0f;
		AngleVectors( view, forward, NULL, NULL );
	}
	dot = DotProduct( forward, dir );
	t->theta = RAD2DEG( acos( Com_Clamp( -1.0f, 1.0f, dot ) ) );
	t->core = RAD2DEG( atan2( AA_TARGET_RADIUS, dist ) ) + AA_CORE_MARGIN;
	t->num = es->number;

	CM_BoxTrace( &trace, eye, t->point, zero, zero, 0, CONTENTS_SOLID, qfalse );
	if ( aa.verbose || cl_aaDebug->integer >= 2 ) {
		Com_Printf( "aa: player %i dist %.0f theta %.1f core %.1f %s at %.0f %.0f %.0f view %.1f %.1f to %.1f %.1f delta %.1f %.1f\n",
			es->number, dist, t->theta, t->core,
			trace.fraction < 1.0f ? "hidden" : "visible", t->point[0], t->point[1], t->point[2],
			AA_View( YAW ), AA_View( PITCH ), t->yaw, t->pitch,
			SHORT2ANGLE( cl.snap.ps.delta_angles[YAW] ), SHORT2ANGLE( cl.snap.ps.delta_angles[PITCH] ) );
	}
	return trace.fraction >= 1.0f;
}

/*
=================
AA_Best

The opponent closest to the view direction, within maxTheta.
=================
*/
static qboolean AA_Best( float maxTheta, aaTarget_t *best ) {
	aaTarget_t t;
	int i;

	best->num = -1;
	for ( i = 0; i < cl.snap.numEntities; i++ ) {
		entityState_t *es = &cl.parseEntities[ ( cl.snap.parseEntitiesNum + i ) & ( MAX_PARSE_ENTITIES - 1 ) ];

		if ( AA_Measure( es, &t ) && t.theta <= maxTheta && ( best->num < 0 || t.theta < best->theta ) ) {
			*best = t;
		}
	}
	return best->num >= 0;
}

/*
=================
AA_Find
=================
*/
static qboolean AA_Find( int num, aaTarget_t *t ) {
	int i;

	for ( i = 0; i < cl.snap.numEntities; i++ ) {
		entityState_t *es = &cl.parseEntities[ ( cl.snap.parseEntitiesNum + i ) & ( MAX_PARSE_ENTITIES - 1 ) ];

		if ( es->number == num ) {
			return AA_Measure( es, t );
		}
	}
	return qfalse;
}

/*
=================
AA_Pick

Keeps the current target while it stays near the view; changes only when
another one is clearly better for a while, so the view does not flick
between two opponents.
=================
*/
static qboolean AA_Pick( aaTarget_t *t ) {
	aaTarget_t best, current;
	qboolean haveCurrent;

	haveCurrent = aa.target >= 0 && AA_Find( aa.target, &current ) &&
		current.theta <= cl_aaFriction->value * AA_KEEP;
	AA_Best( cl_aaFriction->value, &best );

	if ( !haveCurrent ) {
		aa.target = best.num;
		aa.challenger = -1;
		*t = best;
		return best.num >= 0;
	}

	if ( best.num >= 0 && best.num != aa.target && best.theta + AA_SWITCH_ANGLE < current.theta ) {
		if ( aa.challenger != best.num ) {
			aa.challenger = best.num;
			aa.challengerSince = cls.realtime;
		} else if ( cls.realtime - aa.challengerSince >= AA_SWITCH_MS ) {
			aa.target = best.num;
			aa.challenger = -1;
			*t = best;
			return qtrue;
		}
	} else {
		aa.challenger = -1;
	}

	*t = current;
	return qtrue;
}

/*
=================
CL_AimAssistLookScale

Friction: how much of the touch look drag to apply this frame.
=================
*/
float CL_AimAssistLookScale( void ) {
	return aa.lookScale;
}

/*
=================
CL_AimAssistFaceTarget

For LOCK: turns the view to the opponent with the smallest angle inside the
capture cone (or, with nobody there, to the visible one nearest to the view),
so the lock-on of the game picks that one. qfalse if none.
=================
*/
qboolean CL_AimAssistFaceTarget( void ) {
	aaTarget_t t;

	qboolean found;

	if ( cl_aaDebug->integer ) {
		Com_Printf( "aa: LOCK, allowed %i, %i entities in the snapshot\n", AA_Allowed(), cl.snap.numEntities );
	}
	if ( !AA_Allowed() ) {
		return qfalse;
	}
	aa.verbose = cl_aaDebug->integer != 0;
	found = AA_Best( cl_aaCapture->value, &t );
	if ( !found ) {
		// nobody in front: a LOCK tap still turns to the visible opponent
		// nearest to the view, wherever it is, as a lock button does on a pad
		found = AA_Best( 180.0f, &t );
	}
	aa.verbose = qfalse;
	if ( !found ) {
		return qfalse;
	}
	cl.viewangles[YAW] += AngleSubtract( t.yaw, AA_View( YAW ) );
	cl.viewangles[PITCH] += AngleSubtract( t.pitch, AA_View( PITCH ) );
	return qtrue;
}

/*
=================
CL_AimAssist

After the touch controls moved cl.viewangles. lookYaw and lookPitch are the
degrees the thumb turned the view this frame.
=================
*/
void CL_AimAssist( usercmd_t *cmd, float lookYaw, float lookPitch ) {
	aaTarget_t t;
	float dt, k, rate, dyaw, dpitch, len, step;
	qboolean attacking, aiming;
	qboolean assisted = touch_profile->integer != 0;

	dt = ( aa.lastTime && cls.realtime > aa.lastTime ) ? ( cls.realtime - aa.lastTime ) * 0.001f : 0.0f;
	if ( dt > 0.1f ) {
		dt = 0.1f;
	}
	aa.lastTime = cls.realtime;
	aa.lookScale = 1.0f;

	if ( !AA_Allowed() || !AA_Pick( &t ) ) {
		aa.target = -1;
		aa.lastTarget = -1;
		aa.dwell = 0;
		aa.dwellFired = qfalse;
		aa.lastButtons = cmd->buttons;
		return;
	}

	attacking = ( cmd->buttons & ( BUTTON_ATTACK | BUTTON_ALT_ATTACK ) ) != 0;
	aiming = lookYaw != 0.0f || lookPitch != 0.0f;

	// Friction, for the next frames of touch look: from cl_aaSens at the
	// core to 1 at the edge of the friction zone
	if ( cl_aaFriction->value > t.core ) {
		float f = Com_Clamp( 0.0f, 1.0f, ( t.theta - t.core ) / ( cl_aaFriction->value - t.core ) );

		f = f * f * ( 3.0f - 2.0f * f );
		aa.lookScale = cl_aaSens->value + ( 1.0f - cl_aaSens->value ) * f;
	}

	// A quick drag away from it lets go of it for a moment
	if ( dt > 0.0f && aiming ) {
		float speed = sqrt( lookYaw * lookYaw + lookPitch * lookPitch ) / dt;
		float away = lookYaw * AngleSubtract( AA_View( YAW ), t.yaw ) + lookPitch * AngleSubtract( AA_View( PITCH ), t.pitch );

		if ( speed > AA_BREAK_SPEED && away > 0.0f ) {
			aa.breakUntil = cls.realtime + AA_BREAK_MS;
		}
	}

	// Magnet and tracking (the Classic profile keeps only the friction)
	if ( assisted && dt > 0.0f && t.theta < cl_aaMagnet->value && cls.realtime >= aa.breakUntil ) {
		// with no thumb on the look and no attack it only bends the view a little
		k = ( aiming || attacking ) ? 1.0f : cl_aaPassive->value;

		rate = ( t.theta > t.core ) ? 6.0f * t.theta : 0.0f;
		if ( rate > cl_aaPull->value ) {
			rate = cl_aaPull->value;
		}
		dyaw = AngleSubtract( t.yaw, AA_View( YAW ) );
		dpitch = AngleSubtract( t.pitch, AA_View( PITCH ) );
		len = sqrt( dyaw * dyaw + dpitch * dpitch );
		step = k * rate * dt;
		if ( len > 0.0f && step > 0.0f ) {
			if ( step > len ) {
				step = len;
			}
			cl.viewangles[YAW] += dyaw * step / len;
			cl.viewangles[PITCH] += dpitch * step / len;
		}

		// follow its motion across the view: only from the second frame on
		// the same target, lastYaw is stale on the first one
		if ( aa.lastTarget == t.num ) {
			cl.viewangles[YAW] += k * cl_aaTrack->value * AngleSubtract( t.yaw, aa.lastYaw );
			cl.viewangles[PITCH] += k * cl_aaTrack->value * AngleSubtract( t.pitch, aa.lastPitch );
		}
	}
	aa.lastYaw = t.yaw;
	aa.lastPitch = t.pitch;
	aa.lastTarget = t.num;

	// Auto lock: attacking with it in the magnet for a while presses LOCK once.
	// The game may refuse (an opponent already in a fight): it does not insist.
	if ( assisted && attacking && t.theta < cl_aaMagnet->value ) {
		aa.dwell += (int)( dt * 1000.0f );
		if ( !aa.dwellFired && aa.dwell >= cl_aaDwell->integer ) {
			cmd->buttons |= BUTTON_GESTURE;
			aa.dwellFired = qtrue;
		}
	} else if ( !attacking ) {
		aa.dwell = 0;
		aa.dwellFired = qfalse;
	}
}

/*
=================
CL_AimAssistShot

After CL_FinishMove. On the frames an attack starts or is let go (charged
attacks fire on release), the angles of the command are bent onto the
target, up to cl_aaBend degrees. FireWeapon reads ps.viewangles, which come
from this command; the view itself does not move.
=================
*/
void CL_AimAssistShot( usercmd_t *cmd ) {
	aaTarget_t t;
	int changed = ( cmd->buttons ^ aa.lastButtons ) & ( BUTTON_ATTACK | BUTTON_ALT_ATTACK );
	float dyaw, dpitch, len, bend;

	aa.lastButtons = cmd->buttons;
	if ( !changed || !AA_Allowed() || aa.target < 0 || !AA_Find( aa.target, &t ) ) {
		return;
	}
	if ( t.theta > t.core + cl_aaBend->value ) {
		return;
	}

	dyaw = AngleSubtract( t.yaw, AA_View( YAW ) );
	dpitch = AngleSubtract( t.pitch, AA_View( PITCH ) );
	len = sqrt( dyaw * dyaw + dpitch * dpitch );
	bend = len < cl_aaBend->value ? 1.0f : cl_aaBend->value / len;

	cmd->angles[YAW] = ANGLE2SHORT( cl.viewangles[YAW] + dyaw * bend ) & 65535;
	cmd->angles[PITCH] = ANGLE2SHORT( cl.viewangles[PITCH] + dpitch * bend ) & 65535;
}

/*
=================
CL_AimAssistInit
=================
*/
void CL_AimAssistInit( void ) {
	cl_aimAssist = Cvar_Get( "cl_aimAssist", "1", CVAR_ARCHIVE );
	cl_aaCapture = Cvar_Get( "cl_aaCapture", "22", CVAR_ARCHIVE );
	cl_aaFriction = Cvar_Get( "cl_aaFriction", "11", CVAR_ARCHIVE );
	cl_aaSens = Cvar_Get( "cl_aaSens", "0.55", CVAR_ARCHIVE );
	cl_aaMagnet = Cvar_Get( "cl_aaMagnet", "5", CVAR_ARCHIVE );
	cl_aaPull = Cvar_Get( "cl_aaPull", "28", CVAR_ARCHIVE );
	cl_aaPassive = Cvar_Get( "cl_aaPassive", "0.3", CVAR_ARCHIVE );
	cl_aaTrack = Cvar_Get( "cl_aaTrack", "0.6", CVAR_ARCHIVE );
	cl_aaBend = Cvar_Get( "cl_aaBend", "2.5", CVAR_ARCHIVE );
	cl_aaDwell = Cvar_Get( "cl_aaDwell", "400", CVAR_ARCHIVE );
	// 1 prints the players it measures (only on LOCK; 2 also every frame)
	cl_aaDebug = Cvar_Get( "cl_aaDebug", "0", 0 );
	// 1 Asistido: magnet, auto lock and KI in melee (tap blows, hold launch).
	// 0 Clasico: friction only.
	touch_profile = Cvar_Get( "touch_profile", "1", CVAR_ARCHIVE );

	Com_Memset( &aa, 0, sizeof( aa ) );
	aa.target = -1;
	aa.lastTarget = -1;
	aa.challenger = -1;
	aa.lookScale = 1.0f;
}

#endif // USE_TOUCH
