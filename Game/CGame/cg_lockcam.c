/*
===========================================================================
cg_lockcam.c -- dynamic combat camera for lock-on (opt-in via cg_lockCam 1)

Modelled on the Budokai Tenkaichi / Sparking! ZERO "behind the back" battle
camera:

  - Composition first. The fight is framed as a diagonal across the screen:
    the player on one third (lower), the target on the opposite third
    (slightly above center) -- the same framing Sparking uses while a beam
    is being fired. The target is never parked in the middle of the screen.
    cg_lockCamFrameX/Y (and their Melee variants) say where the two go.
  - The camera ORBITS the player. The orbit direction is the 3D line from
    the enemy through the player (height included), rotated by the yaw and
    elevation offsets that produce the composition above. Those offsets are
    solved from the screen positions, the camera distance and the distance
    between the fighters (CG_LockCam_OrbitOffset), so the framing holds at
    melee range and at 17000 units alike.
  - Only ANGLES are smoothed: the orbit (cinematic lag when circling the
    enemy), the view direction (so a twitchy target does not shake the
    whole screen), the melee/ranged blend and the shoulder side. The pivot
    is always the current player position, so the camera never falls behind
    a player moving at thousands of units per second.
  - If the composition cannot fit both fighters (enemy far above/below), the
    view slides toward the player and the orbit zooms out.
  - The shoulder side follows the player's lateral movement direction relative
    to the target. The camera smoothly interpolates toward the side the player
    is moving toward (left/right). When the player stops moving laterally,
    the camera stays at the last reached extreme (defined by cg_lockCamFrameX).
  - Switching between this camera and the regular one (lock-on start/end,
    transformations, lock dropped by the server) is blended instead of cut
    (CG_LockCam_Transition).

Everything here is client-side only (cgame): the server still aims the
player at the target exactly as before, this only places the viewer camera.
The legacy camera stays untouched behind cg_lockCam 0 (the default).
===========================================================================
*/
#include "cg_local.h"

// Distance between the fighters (game units) at/under which the framing is
// fully "melee", and at/over which it is fully "ranged". Melee exchanges in
// this game happen at 30-60 units; most ranged fighting happens at 2000+.
#define LOCKCAM_MELEE_RANGE		150.0f
#define LOCKCAM_FAR_RANGE		800.0f
// Time to blend between melee and ranged framing: a knockback sends the
// enemy from 40 to 2000 units in a fraction of a second, which would
// otherwise snap the composition.
#define LOCKCAM_RANGE_SMOOTH	0.50f

// Heights relative to playerState_t/entityState_t origin (the center of the
// bounding box, see bg_pmove.c: mins z = MINS_Z, maxs z = 32). Used to frame
// the actual body instead of a single point.
#define LOCKCAM_BODY_HEIGHT		8.0f
#define LOCKCAM_HEAD_HEIGHT		30.0f
#define LOCKCAM_FEET_HEIGHT		-20.0f

// Screen height of the target, as a fraction of half the screen (+ = up).
#define LOCKCAM_TARGET_Y		0.05f

// Limits on the solved orbit offsets. Past these the camera would be side-on
// (controls stop reading as "forward = toward the enemy") or straight above.
#define LOCKCAM_ORBIT_YAW_MAX	70.0f
#define LOCKCAM_ORBIT_ELEV_MAX	40.0f
// How far above/below the player the orbit may go in total. Past this the
// camera would be looking almost straight up/down, where yaw is unstable.
#define LOCKCAM_ORBIT_PITCH_MAX	55.0f
// Last-resort clamp on the final view pitch (guards vectoangles() near
// vertical; the orbit clamp above keeps this from being reached normally).
#define LOCKCAM_VIEW_PITCH_MAX	85.0f

// Fraction of the half-FOV (per axis) that subjects must stay inside.
#define LOCKCAM_FRAME_X			0.85f
#define LOCKCAM_FRAME_Y			0.85f

// Zoom-out used when both fighters cannot fit at the base distance.
#define LOCKCAM_ZOOM_MAX		3.0f
#define LOCKCAM_ZOOM_SMOOTH		0.50f

// Orbit lag (degrees) past which the rotation smoothing starts stiffening.
#define LOCKCAM_LAG_SOFT		25.0f

// Collision: pull in instantly, ease back out over this long.
#define LOCKCAM_COLLIDE_SMOOTH	0.30f

// Shoulder side switching.
#define LOCKCAM_SIDE_SMOOTH		0.35f	// seconds for the camera to cross to the other shoulder
#define LOCKCAM_SIDE_LIFT		10.0f	// extra elevation (degrees) while crossing behind the player

// Blend between this camera and the regular third person camera.
#define LOCKCAM_TRANSITION_MS	450

#define LOCKCAM_MASK_CAMERACLIP	(MASK_SOLID|CONTENTS_PLAYERCLIP)
static vec3_t lockcam_mins = { -4, -4, -4 };
static vec3_t lockcam_maxs = {  4,  4,  4 };

// ---- persistent state ------------------------------------------------
static qboolean	lockcam_active		= qfalse;	// were we driving the camera last frame?
static int		lockcam_lastTime	= 0;

static float	lockcam_yaw			= 0.0f;		// smoothed orbit yaw (degrees)
static float	lockcam_yawVel		= 0.0f;
static float	lockcam_pitch		= 0.0f;		// smoothed orbit elevation (degrees, + = camera above)
static float	lockcam_pitchVel	= 0.0f;
static float	lockcam_viewYaw		= 0.0f;		// smoothed view direction (Q3 angles)
static float	lockcam_viewYawVel	= 0.0f;
static float	lockcam_viewPitch	= 0.0f;
static float	lockcam_viewPitchVel = 0.0f;
static float	lockcam_viewRatio	= 0.0f;		// how well the smoothed view framed last frame
static float	lockcam_range		= 0.0f;		// smoothed melee (0) .. ranged (1) blend
static float	lockcam_rangeVel	= 0.0f;
static float	lockcam_zoom		= 1.0f;		// distance multiplier from the composer
static float	lockcam_zoomVel		= 0.0f;
static float	lockcam_colFrac		= 1.0f;		// collision pull-in fraction
static float	lockcam_colVel		= 0.0f;

// Last well-defined orbit yaw, used while the enemy is (almost) straight
// above/below the player and the horizontal direction is undefined.
static float	lockcam_lastBaseYaw	= 0.0f;

// Shoulder side: +1 = camera over the right shoulder (player on the left of
// the screen, target on the right), -1 = mirrored. lockcam_sideBlend eases
// between the two.
static float	lockcam_side		= 1.0f;
static float	lockcam_sideBlend	= 1.0f;
static float	lockcam_sideVel		= 0.0f;		// velocity for lockcam_side (target) smoothing
static float	lockcam_sideBlendVel	= 0.0f;		// velocity for lockcam_sideBlend (final) smoothing

// Transition blend with the regular camera (see CG_LockCam_Transition).
static qboolean	lockcam_haveLast	= qfalse;
static qboolean	lockcam_lastWasLockCam = qfalse;
static vec3_t	lockcam_lastOrgRel;				// last final camera, relative to the player origin
static vec3_t	lockcam_lastAngles;
static int		lockcam_transStart	= 0;
static vec3_t	lockcam_transFromRel;
static vec3_t	lockcam_transFromAngles;

// ---- debug snapshot (filled every frame we run, read by the overlay) --
static vec3_t	lockcam_dbgPlayer;
static vec3_t	lockcam_dbgEnemy;
static float	lockcam_dbgDist;
static float	lockcam_dbgYawOff;
static float	lockcam_dbgYawLag;
static float	lockcam_dbgFit;
static float	lockcam_dbgPlayerRatio;
static float	lockcam_dbgEnemyRatio;
static int		lockcam_dbgLastPrint = 0;


/*
==============
CG_LockCam_SmoothDamp

Critically damped spring smoothing without calling exp()/pow() -- neither
is available to the QVM build (see bg_lib.c). This is the same rational
approximation of exp(-x) that Unity Mathf.SmoothDamp uses for exactly
this reason: 1/(1+x+0.48x^2+0.235x^3) tracks e^-x closely for the x ranges
a per-frame smoothing update produces, using only add/sub/mul/div.

Returns the new value; *vel carries the spring momentum between calls.
==============
*/
static float CG_LockCam_SmoothDamp( float current, float target, float *vel, float smoothTime, float dt ) {
	float omega, x, expApprox;
	float change, originalTo, temp, output;

	if ( smoothTime < 0.0001f ) {
		smoothTime = 0.0001f;
	}
	if ( dt <= 0.0f ) {
		return current;
	}

	omega = 2.0f / smoothTime;
	x = omega * dt;
	expApprox = 1.0f / ( 1.0f + x + 0.48f * x * x + 0.235f * x * x * x );

	change = current - target;
	originalTo = target;

	temp = ( *vel + omega * change ) * dt;
	*vel = ( *vel - omega * temp ) * expApprox;
	output = target + ( change + temp ) * expApprox;

	// Prevent overshoot past the target (can happen on a big dt spike).
	if ( ( originalTo - current > 0.0f ) == ( output > originalTo ) ) {
		output = originalTo;
		*vel = ( output - originalTo ) / dt;
	}

	return output;
}

/*
==============
CG_LockCam_SmoothDampAngle

Same as above for an angle in degrees, taking the short way around.
==============
*/
static float CG_LockCam_SmoothDampAngle( float current, float target, float *vel, float smoothTime, float dt ) {
	target = current + AngleSubtract( target, current );
	return AngleNormalize360( CG_LockCam_SmoothDamp( current, target, vel, smoothTime, dt ) );
}

/*
==============
CG_LockCam_FovY

Mirrors the fov_x -> fov_y derivation in CG_CalcFov(), so framing uses the
vertical FOV the renderer will actually use for this aspect ratio (without
cg_widescreenFov it gets small on ultrawide: fov 90 at 21:9 leaves ~45
degrees vertically).
==============
*/
static float CG_LockCam_FovY( float fovXDeg ) {
	float x, fovY;

	if ( cg.refdef.width <= 0 || cg.refdef.height <= 0 ) {
		return fovXDeg;	// defensive; CG_CalcVrect() always runs first in practice
	}

	x = cg.refdef.width / tan( fovXDeg / 360.0f * M_PI );
	fovY = atan2( (float)cg.refdef.height, x );
	return fovY * 360.0f / M_PI;
}

/*
==============
CG_LockCam_DirAngles

View angles for a direction, pitch normalized to -180..180 and clamped.
==============
*/
static void CG_LockCam_DirAngles( const vec3_t dir, vec3_t outAngles ) {
	vectoangles( dir, outAngles );
	outAngles[PITCH] = AngleNormalize180( outAngles[PITCH] );
	if ( outAngles[PITCH] > LOCKCAM_VIEW_PITCH_MAX ) {
		outAngles[PITCH] = LOCKCAM_VIEW_PITCH_MAX;
	} else if ( outAngles[PITCH] < -LOCKCAM_VIEW_PITCH_MAX ) {
		outAngles[PITCH] = -LOCKCAM_VIEW_PITCH_MAX;
	}
	outAngles[ROLL] = 0.0f;
}

/*
==============
CG_LockCam_FrameRatio

How far outside the allowed part of the screen the given points project for
this camera: <= 1 means all of them are inside, > 1 means at least one is
out by that factor. Works in screen space (tangent of the angle), so it is
exact for a pinhole camera on both axes.
==============
*/
static float CG_LockCam_FrameRatio( const vec3_t camPos, const vec3_t angles, const vec3_t *points, int numPoints,
									 float tanLimitX, float tanLimitY ) {
	vec3_t	fwd, right, up, rel;
	float	depth, rx, ry, ratio;
	int		i;

	AngleVectors( angles, fwd, right, up );

	ratio = 0.0f;
	for ( i = 0; i < numPoints; i++ ) {
		VectorSubtract( points[i], camPos, rel );
		depth = DotProduct( rel, fwd );
		if ( depth < 1.0f ) {
			return 99.0f;	// behind the camera: definitely not in frame
		}
		rx = Q_fabs( DotProduct( rel, right ) / depth ) / tanLimitX;
		ry = Q_fabs( DotProduct( rel, up ) / depth ) / tanLimitY;
		if ( rx > ratio ) {
			ratio = rx;
		}
		if ( ry > ratio ) {
			ratio = ry;
		}
	}
	return ratio;
}

/*
==============
CG_LockCam_OrbitOffset

How far (degrees) the camera has to swing off the fight line so that, seen
from the camera, the player and the enemy are sepDeg apart. With the camera
camDist behind the player, the enemy fightDist beyond it and the camera
swung by y, the angle between them is atan(D sin y / (d + D cos y)); solving
for y gives y = sep + asin((d / D) sin sep). Far away that is just sep; up
close the camera has to swing much wider to open the same gap, and past a
point the gap cannot be opened at all -- hence the clamp.
==============
*/
static float CG_LockCam_OrbitOffset( float sepDeg, float camDist, float fightDist, float maxDeg ) {
	float s, off;

	if ( fightDist < 1.0f ) {
		fightDist = 1.0f;
	}
	s = ( camDist / fightDist ) * sin( DEG2RAD( sepDeg ) );
	if ( s > 1.0f ) {
		s = 1.0f;
	} else if ( s < -1.0f ) {
		s = -1.0f;
	}
	// asin(s) without asin (not in bg_lib.c)
	off = sepDeg + RAD2DEG( atan2( s, sqrt( 1.0f - s * s ) ) );
	if ( off > maxDeg ) {
		off = maxDeg;
	} else if ( off < -maxDeg ) {
		off = -maxDeg;
	}
	return off;
}

/*
==============
CG_LockCam_OrbitPos

Camera position for an orbit (yaw, elevation) around pivot at dist.
==============
*/
static void CG_LockCam_OrbitPos( const vec3_t pivot, float yaw, float elev, float dist, vec3_t out ) {
	float	cp;
	vec3_t	dir;

	cp = cos( DEG2RAD( elev ) );
	dir[0] = cp * cos( DEG2RAD( yaw ) );
	dir[1] = cp * sin( DEG2RAD( yaw ) );
	dir[2] = sin( DEG2RAD( elev ) );
	VectorMA( pivot, dist, dir, out );
}

/*
==============
CG_LockCam_UpdateSide

Two-stage shoulder following:
  1. lockcam_side (target) smoothly follows lateral movement direction
     toward ±1 using cg_lockCamSideFollowSpeed.
  2. lockcam_sideBlend follows lockcam_side using LOCKCAM_SIDE_SMOOTH.
     When target is at extreme (±1.0), stage 2 speeds up to ensure
     the camera reaches the full cg_lockCamFrameX framing.
When lateral movement stops, lockcam_side holds its position (doesn't return to center).
==============
*/
static qboolean CG_LockCam_UpdateSide( float baseYaw, float yawOff, const vec3_t pivot, float elev, float orbitDist,
									   float lateralSpeed, int clientNum, float dt ) {
	float targetSide;
	float stage2Smooth = LOCKCAM_SIDE_SMOOTH;

	if ( !cg_lockCamAutoSide.integer ) {
		return qfalse;
	}

	// Determine target side from lateral movement direction.
	// lateralSpeed > 0 = player moving to its right relative to target
	// lateralSpeed < 0 = player moving to its left relative to target
	// We want the camera on the opposite shoulder so the player frames
	// on the side they're moving toward.
	if ( Q_fabs( lateralSpeed ) > 10.0f ) {
		targetSide = ( lateralSpeed > 0.0f ) ? -1.0f : 1.0f;
		// Target is at extreme: speed up stage 2 so camera reaches full
		// cg_lockCamFrameX framing during the movement.
		stage2Smooth = LOCKCAM_SIDE_SMOOTH * 0.3f;  // ~0.1s instead of 0.35s
	} else {
		// No significant lateral movement: hold current target side (don't return to center)
		targetSide = lockcam_side;
	}

	// Stage 1: lockcam_side (target) smoothly follows the movement direction.
	lockcam_side = CG_LockCam_SmoothDamp( lockcam_side, targetSide, &lockcam_sideVel,
										  cg_lockCamSideFollowSpeed.value, dt );

	// Stage 2: lockcam_sideBlend follows the target.
	// Faster when targeting extreme so camera reaches full framing.
	lockcam_sideBlend = CG_LockCam_SmoothDamp( lockcam_sideBlend, lockcam_side, &lockcam_sideBlendVel,
											   stage2Smooth, dt );

	// Clamp to [-1, 1] for safety.
	if ( lockcam_sideBlend > 1.0f ) lockcam_sideBlend = 1.0f;
	if ( lockcam_sideBlend < -1.0f ) lockcam_sideBlend = -1.0f;

	return qfalse;
}

/*
==============
CG_OffsetLockedCombatView

Called from CG_CalcViewValues() when cg_lockCam 1 and the player has a live
lock-on target. Returns qfalse (without touching the refdef) for the states
this camera leaves to the regular one -- mid-transform, whose cinematic
camera drives cg_thirdPerson* cvars directly, and an invalid target -- and
the caller falls back to CG_OffsetThirdPersonView[2](). Beam guiding is
handled here (it used to fall back too, which is what cut the view the
moment a guided beam like a kamehameha was released).
==============
*/
qboolean CG_OffsetLockedCombatView( void ) {
	playerState_t	*ps;
	int				clientNum, targetNum;
	centity_t		*targetCent;
	vec3_t			player, enemy, playerPoints[3], toEnemy, back, fwdFlat;
	vec3_t			pivot, camFull, camPos, dir, dirP, dirE;
	vec3_t			anglesE, anglesB, target, view, test;
	float			dist, t, horiz, baseYaw, baseElev;
	float			idealYaw, idealPitch, baseDist, orbitDist;
	float			fovX, fovY, tanH, tanV, tanLimitX, tanLimitY;
	float			frameX, frameY, alphaE, betaE, betaP, yawOff, elevOff;
	float			fit, lo, hi, mid, playerRatio, enemyRatio, need, zoomTarget;
	float			dt, colTarget, lookSmooth;
	qboolean		snap;
	trace_t			trace;
	int				i;

	ps = &cg.predictedPlayerState;
	clientNum = ps->clientNum;

	if ( ps->timers[tmTransform] > 1 ) {
		lockcam_active = qfalse;
		return qfalse;
	}
	targetNum = ps->lockedTarget - 1;
	if ( targetNum < 0 || targetNum >= MAX_CLIENTS || !cgs.clientinfo[targetNum].infoValid ||
		 !cg_entities[targetNum].currentValid ) {
		lockcam_active = qfalse;
		return qfalse;
	}
	targetCent = &cg_entities[targetNum];

	// CG_CalcViewValues() (which calls us) runs before CG_AddPacketEntities()
	// updates every entity lerpOrigin for this frame -- without this call
	// the camera would be framing where the enemy was last frame. Safe to
	// call again later in the frame; see the comment on the definition.
	CG_CalcEntityLerpPositions( targetCent );

	VectorCopy( ps->origin, player );
	player[2] += LOCKCAM_BODY_HEIGHT;
	VectorCopy( targetCent->lerpOrigin, enemy );
	enemy[2] += LOCKCAM_BODY_HEIGHT;

	VectorCopy( ps->origin, playerPoints[0] );
	playerPoints[0][2] += LOCKCAM_HEAD_HEIGHT;
	VectorCopy( ps->origin, playerPoints[1] );
	playerPoints[1][2] += LOCKCAM_FEET_HEIGHT;
	VectorCopy( player, playerPoints[2] );

	VectorSubtract( enemy, player, toEnemy );
	dist = VectorLength( toEnemy );

	// --- the fight line ---
	if ( dist > 1.0f ) {
		VectorScale( toEnemy, -1.0f / dist, back );
	} else {
		AngleVectors( ps->viewangles, back, NULL, NULL );
		VectorScale( back, -1.0f, back );
	}
	horiz = sqrt( back[0] * back[0] + back[1] * back[1] );
	if ( horiz > 0.05f ) {
		baseYaw = RAD2DEG( atan2( back[1], back[0] ) );
		lockcam_lastBaseYaw = baseYaw;
	} else {
		baseYaw = lockcam_lastBaseYaw;	// enemy straight above/below: keep the last heading
	}
	baseElev = RAD2DEG( atan2( back[2], horiz ) );	// + when the player is above the enemy

	// --- timing / (re)entry ---
	dt = ( cg.time - lockcam_lastTime ) * 0.001f;
	snap = ( !lockcam_active || cg.thisFrameTeleport || dt <= 0.0f || dt > 0.5f );

	t = ( dist - LOCKCAM_MELEE_RANGE ) / ( LOCKCAM_FAR_RANGE - LOCKCAM_MELEE_RANGE );
	if ( t < 0.0f ) {
		t = 0.0f;
	} else if ( t > 1.0f ) {
		t = 1.0f;
	}

	if ( snap ) {
		lockcam_range = t;
		lockcam_zoom = 1.0f;
		lockcam_colFrac = 1.0f;
		lockcam_rangeVel = lockcam_zoomVel = lockcam_colVel = 0.0f;
		lockcam_yawVel = lockcam_pitchVel = lockcam_viewYawVel = lockcam_viewPitchVel = 0.0f;
		lockcam_viewRatio = 0.0f;

		// Start on the shoulder the previous camera was already on, so
		// locking on does not swing the camera across the player.
		lockcam_side = 1.0f;
		if ( lockcam_haveLast ) {
			fwdFlat[0] = -back[0];
			fwdFlat[1] = -back[1];
			fwdFlat[2] = 0.0f;
			if ( VectorNormalize( fwdFlat ) > 0.01f ) {
				// right of the player facing the enemy is (fwd.y, -fwd.x)
				if ( lockcam_lastOrgRel[0] * fwdFlat[1] - lockcam_lastOrgRel[1] * fwdFlat[0] < 0.0f ) {
					lockcam_side = -1.0f;
				}
			}
		}
lockcam_sideBlend = lockcam_side;
		lockcam_sideVel = 0.0f;
		lockcam_sideBlendVel = 0.0f;
		dt = 0.0f;
	} else {
		lockcam_range = CG_LockCam_SmoothDamp( lockcam_range, t, &lockcam_rangeVel, LOCKCAM_RANGE_SMOOTH, dt );
	}
	t = lockcam_range;

	baseDist = cg_lockCamRangeMelee.value + t * ( cg_lockCamRange.value - cg_lockCamRangeMelee.value );
	orbitDist = baseDist * lockcam_zoom;

	// --- composition -> orbit offsets ---
	fovX = cg_fov.value;
	if ( fovX < 1.0f ) {
		fovX = 1.0f;
	} else if ( fovX > 160.0f ) {
		fovX = 160.0f;
	}
	fovX = CG_WidescreenFovX( fovX );	// same FOV CG_CalcFov() will hand the renderer
	fovY = CG_LockCam_FovY( fovX );
	tanH = tan( DEG2RAD( fovX * 0.5f ) );
	tanV = tan( DEG2RAD( fovY * 0.5f ) );
	tanLimitX = tanH * LOCKCAM_FRAME_X;
	tanLimitY = tanV * LOCKCAM_FRAME_Y;

	frameX = cg_lockCamFrameXMelee.value + t * ( cg_lockCamFrameX.value - cg_lockCamFrameXMelee.value );
	frameY = cg_lockCamFrameYMelee.value + t * ( cg_lockCamFrameY.value - cg_lockCamFrameYMelee.value );
	alphaE = RAD2DEG( atan2( frameX * tanH, 1.0f ) );						// target this far off center, horizontally
	betaE = RAD2DEG( atan2( LOCKCAM_TARGET_Y * tanV, 1.0f ) );				// target above center
	betaP = RAD2DEG( atan2( ( LOCKCAM_TARGET_Y - frameY ) * tanV, 1.0f ) );	// player below it
	yawOff = CG_LockCam_OrbitOffset( 2.0f * alphaE, orbitDist, dist, LOCKCAM_ORBIT_YAW_MAX );
	elevOff = CG_LockCam_OrbitOffset( betaE - betaP, orbitDist, dist, LOCKCAM_ORBIT_ELEV_MAX );

	VectorCopy( player, pivot );
	pivot[2] += cg_lockCamHeight.value;

	// --- shoulder side ---
	if ( !snap ) {
		float	lateralSpeed = 0.0f;
		vec3_t	enemyVel, relVel;

		// Target velocity: BG_PlayerStateToEntityState() sends the player
		// velocity in pos.trDelta, interpolated here like its position.
		if ( targetCent->interpolate ) {
			for ( i = 0; i < 3; i++ ) {
				enemyVel[i] = targetCent->currentState.pos.trDelta[i] + cg.frameInterpolation *
					( targetCent->nextState.pos.trDelta[i] - targetCent->currentState.pos.trDelta[i] );
			}
		} else {
			VectorCopy( targetCent->currentState.pos.trDelta, enemyVel );
		}
		VectorSubtract( ps->velocity, enemyVel, relVel );

		// relative sideways speed, + = player moving toward its right (or the
		// target toward the player left) while facing the target
		fwdFlat[0] = -back[0];
		fwdFlat[1] = -back[1];
		fwdFlat[2] = 0.0f;
		if ( VectorNormalize( fwdFlat ) > 0.01f ) {
			lateralSpeed = relVel[0] * fwdFlat[1] - relVel[1] * fwdFlat[0];
		}
		CG_LockCam_UpdateSide( baseYaw, yawOff, pivot, lockcam_pitch, orbitDist, lateralSpeed, clientNum, dt );
	}

	// --- ideal orbit ---
	idealYaw = baseYaw + lockcam_sideBlend * yawOff;
	idealPitch = baseElev + elevOff + LOCKCAM_SIDE_LIFT * ( 1.0f - Q_fabs( lockcam_sideBlend ) );
	if ( idealPitch > LOCKCAM_ORBIT_PITCH_MAX ) {
		idealPitch = LOCKCAM_ORBIT_PITCH_MAX;
	} else if ( idealPitch < -LOCKCAM_ORBIT_PITCH_MAX ) {
		idealPitch = -LOCKCAM_ORBIT_PITCH_MAX;
	}

	// --- orbit smoothing (angles only) ---
	if ( snap ) {
		lockcam_yaw = AngleNormalize360( idealYaw );
		lockcam_pitch = idealPitch;
	} else {
		float smoothTime = cg_lockCamSmooth.value;
		float lag = Q_fabs( AngleSubtract( idealYaw, lockcam_yaw ) );

		// The lag stiffens as it grows: small lags (normal strafing) keep the
		// full cg_lockCamSmooth drift, big ones (circling at full speed,
		// zanzoken behind the player, flying over the enemy) shrink the
		// smoothing time. Continuous on purpose -- a hard clamp would cut.
		if ( lag > LOCKCAM_LAG_SOFT ) {
			float scale = LOCKCAM_LAG_SOFT / lag;
			if ( scale < 0.25f ) {
				scale = 0.25f;
			}
			smoothTime *= scale;
		}
		lockcam_yaw = CG_LockCam_SmoothDampAngle( lockcam_yaw, idealYaw, &lockcam_yawVel, smoothTime, dt );
		lockcam_pitch = CG_LockCam_SmoothDamp( lockcam_pitch, idealPitch, &lockcam_pitchVel, smoothTime, dt );
	}
	lockcam_active = qtrue;
	lockcam_lastTime = cg.time;

	// --- camera position: current player position + smoothed orbit ---
	CG_LockCam_OrbitPos( pivot, lockcam_yaw, lockcam_pitch, orbitDist, camFull );

	// Collision: pull in immediately, ease back out (no popping).
	CG_Trace( &trace, pivot, lockcam_mins, lockcam_maxs, camFull, clientNum, LOCKCAM_MASK_CAMERACLIP );
	colTarget = trace.startsolid ? lockcam_colFrac : trace.fraction;
	if ( colTarget < lockcam_colFrac || snap ) {
		lockcam_colFrac = colTarget;
		lockcam_colVel = 0.0f;
	} else {
		lockcam_colFrac = CG_LockCam_SmoothDamp( lockcam_colFrac, colTarget, &lockcam_colVel, LOCKCAM_COLLIDE_SMOOTH, dt );
	}
	for ( i = 0; i < 3; i++ ) {
		camPos[i] = pivot[i] + ( camFull[i] - pivot[i] ) * lockcam_colFrac;
	}

	// --- target view: the target on its third of the screen ---
	VectorSubtract( enemy, camPos, dirE );
	if ( VectorNormalize( dirE ) < 0.001f ) {
		VectorScale( back, -1.0f, dirE );
	}
	CG_LockCam_DirAngles( dirE, anglesE );
	target[YAW] = anglesE[YAW] + lockcam_sideBlend * alphaE;	// yaw grows to the left: target ends up on the right for side +1
	target[PITCH] = anglesE[PITCH] + betaE;						// pitch grows downward: target ends up above center
	target[ROLL] = 0.0f;

	// If the composition does not fit the whole player (enemy far above or
	// below), slide the view toward the bisector between the two until it
	// does. Found by binary search on the target view only; the smoothing
	// below turns the result into continuous motion.
	fit = 0.0f;
	playerRatio = CG_LockCam_FrameRatio( camPos, target, (const vec3_t *)playerPoints, 3, tanLimitX, tanLimitY );
	if ( playerRatio > 1.0f ) {
		VectorSubtract( player, camPos, dirP );
		VectorNormalize( dirP );
		VectorAdd( dirP, dirE, dir );
		if ( VectorNormalize( dir ) < 0.001f ) {
			VectorCopy( dirE, dir );
		}
		CG_LockCam_DirAngles( dir, anglesB );

		lo = 0.0f;
		hi = 1.0f;
		for ( i = 0; i < 8; i++ ) {
			mid = ( lo + hi ) * 0.5f;
			test[YAW] = LerpAngle( target[YAW], anglesB[YAW], mid );
			test[PITCH] = LerpAngle( target[PITCH], anglesB[PITCH], mid );
			test[ROLL] = 0.0f;
			if ( CG_LockCam_FrameRatio( camPos, test, (const vec3_t *)playerPoints, 3, tanLimitX, tanLimitY ) <= 1.0f ) {
				hi = mid;
			} else {
				lo = mid;
			}
		}
		fit = hi;
		target[YAW] = LerpAngle( target[YAW], anglesB[YAW], fit );
		target[PITCH] = LerpAngle( target[PITCH], anglesB[PITCH], fit );
		playerRatio = CG_LockCam_FrameRatio( camPos, target, (const vec3_t *)playerPoints, 3, tanLimitX, tanLimitY );
	}
	enemyRatio = CG_LockCam_FrameRatio( camPos, target, (const vec3_t *)&enemy, 1, tanLimitX, tanLimitY );

	// Zoom out while the two do not fit; relax once there is room. Judged on
	// the target view (stable), with a dead band so it does not pump.
	need = ( playerRatio > enemyRatio ) ? playerRatio : enemyRatio;
	if ( need >= 5.0f ) {
		zoomTarget = lockcam_zoom;	// someone behind the camera mid-swing: the swing fixes it
	} else if ( need > 1.0f ) {
		zoomTarget = lockcam_zoom * need * 1.05f;
	} else if ( need < 0.75f ) {
		zoomTarget = 1.0f;
	} else {
		zoomTarget = lockcam_zoom;
	}
	if ( zoomTarget > LOCKCAM_ZOOM_MAX ) {
		zoomTarget = LOCKCAM_ZOOM_MAX;
	} else if ( zoomTarget < 1.0f ) {
		zoomTarget = 1.0f;
	}
	lockcam_zoom = CG_LockCam_SmoothDamp( lockcam_zoom, zoomTarget, &lockcam_zoomVel, LOCKCAM_ZOOM_SMOOTH, dt );

	// --- view smoothing ---
	// Aiming straight at the target every frame turned every twitch of the
	// target (bots dodge constantly, positions arrive at snapshot rate) into
	// a shake of the whole screen. Ease toward the target view instead; if
	// that ever lets someone reach the real edge of the screen, catch up
	// faster until they are back.
	if ( snap ) {
		lockcam_viewYaw = AngleNormalize360( target[YAW] );
		lockcam_viewPitch = target[PITCH];
	} else {
		lookSmooth = cg_lockCamSmoothLook.value;
		if ( lockcam_viewRatio > 1.0f / LOCKCAM_FRAME_X ) {
			lookSmooth *= 0.35f;
		}
		lockcam_viewYaw = CG_LockCam_SmoothDampAngle( lockcam_viewYaw, target[YAW], &lockcam_viewYawVel, lookSmooth, dt );
		lockcam_viewPitch = CG_LockCam_SmoothDamp( lockcam_viewPitch, target[PITCH], &lockcam_viewPitchVel, lookSmooth, dt );
	}
	view[YAW] = lockcam_viewYaw;
	view[PITCH] = lockcam_viewPitch;
	view[ROLL] = 0.0f;
	{
		float pr = CG_LockCam_FrameRatio( camPos, view, (const vec3_t *)playerPoints, 3, tanLimitX, tanLimitY );
		float er = CG_LockCam_FrameRatio( camPos, view, (const vec3_t *)&enemy, 1, tanLimitX, tanLimitY );
		lockcam_viewRatio = ( pr > er ) ? pr : er;
	}

	// --- commit to the refdef ---
	VectorCopy( view, cg.refdefViewAngles );
	VectorCopy( camPos, cg.refdef.vieworg );

	// --- debug snapshot for CG_LockCam_DrawDebug() ---
	VectorCopy( player, lockcam_dbgPlayer );
	VectorCopy( enemy, lockcam_dbgEnemy );
	lockcam_dbgDist = dist;
	lockcam_dbgYawOff = yawOff;
	lockcam_dbgYawLag = AngleSubtract( idealYaw, lockcam_yaw );
	lockcam_dbgFit = fit;
	lockcam_dbgPlayerRatio = playerRatio;
	lockcam_dbgEnemyRatio = enemyRatio;
	return qtrue;
}

/*
==============
CG_LockCam_Transition

Called from CG_CalcViewValues() after the third person camera has been
placed, whichever camera placed it. When the camera in charge changes
between the lock-on camera and the regular one (lock-on starts or ends,
a transformation takes over, the server drops the lock), eases from the
last frame of the old one into the new one over LOCKCAM_TRANSITION_MS
instead of cutting. The starting point is kept relative to the player so
it travels with them during the blend.
==============
*/
void CG_LockCam_Transition( qboolean lockCamDrove ) {
	vec3_t	base, from;
	float	u, e;
	int		i;

	if ( !lockCamDrove ) {
		lockcam_active = qfalse;
	}
	if ( !cg_lockCam.integer || !cg.renderingThirdPerson || cg.thisFrameTeleport ) {
		lockcam_haveLast = qfalse;
		lockcam_transStart = 0;
		return;
	}

	VectorCopy( cg.predictedPlayerState.origin, base );

	if ( lockcam_haveLast && lockCamDrove != lockcam_lastWasLockCam ) {
		lockcam_transStart = cg.time;
		VectorCopy( lockcam_lastOrgRel, lockcam_transFromRel );
		VectorCopy( lockcam_lastAngles, lockcam_transFromAngles );
	}

	if ( lockcam_transStart && cg.time >= lockcam_transStart && cg.time - lockcam_transStart < LOCKCAM_TRANSITION_MS ) {
		u = (float)( cg.time - lockcam_transStart ) / (float)LOCKCAM_TRANSITION_MS;
		e = u * u * ( 3.0f - 2.0f * u );	// smoothstep: eases in and out
		VectorAdd( base, lockcam_transFromRel, from );
		for ( i = 0; i < 3; i++ ) {
			cg.refdef.vieworg[i] = from[i] + e * ( cg.refdef.vieworg[i] - from[i] );
			cg.refdefViewAngles[i] = LerpAngle( lockcam_transFromAngles[i], cg.refdefViewAngles[i], e );
		}
	} else {
		lockcam_transStart = 0;
	}

	VectorSubtract( cg.refdef.vieworg, base, lockcam_lastOrgRel );
	VectorCopy( cg.refdefViewAngles, lockcam_lastAngles );
	lockcam_lastWasLockCam = lockCamDrove;
	lockcam_haveLast = qtrue;
}

/*
==============
CG_LockCam_DrawDebug

cg_lockCamDebug 1 overlay: projects the player (green) and the enemy (red)
to the screen, draws the allowed framing box, and prints the framing
numbers to the console. No-op unless the cvar is set and the dynamic combat
camera actually ran this frame.
==============
*/
void CG_LockCam_DrawDebug( void ) {
	float	x, y;
	char	buf[192];

	if ( !cg_lockCamDebug.integer || !lockcam_active || !cg.snap ) {
		return;
	}
	if ( cg.predictedPlayerState.lockedTarget <= 0 ) {
		return;
	}

	if ( CG_WorldCoordToScreenCoordFloat( lockcam_dbgPlayer, &x, &y ) ) {
		CG_FillRect( x - 4, y - 4, 8, 8, colorGreen );
	}
	if ( CG_WorldCoordToScreenCoordFloat( lockcam_dbgEnemy, &x, &y ) ) {
		CG_FillRect( x - 4, y - 4, 8, 8, colorRed );
	}

	// Allowed framing box (screen-space, same fractions the composer uses).
	{
		float mx = 320.0f * ( 1.0f - LOCKCAM_FRAME_X );
		float my = 240.0f * ( 1.0f - LOCKCAM_FRAME_Y );
		CG_DrawRect( mx, my, 640.0f - 2.0f * mx, 480.0f - 2.0f * my, 1.0f, colorCyan );
	}

	// Numeric readout goes to the console instead of on-screen text: the
	// glyph-drawing helpers (CG_Text_Paint and friends) only compile under
	// MISSIONPACK in this codebase. Throttled so it stays readable.
	if ( cg.time - lockcam_dbgLastPrint > 500 ) {
		lockcam_dbgLastPrint = cg.time;
		Com_sprintf( buf, sizeof( buf ),
			"lockcam dist:%.0f range:%.2f side:%+.0f/%+.2f yawOff:%.1f yawLag:%.1f fit:%.2f zoom:%.2f col:%.2f player:%.2f enemy:%.2f\n",
			lockcam_dbgDist, lockcam_range, lockcam_side, lockcam_sideBlend, lockcam_dbgYawOff,
			lockcam_dbgYawLag, lockcam_dbgFit, lockcam_zoom, lockcam_colFrac, lockcam_dbgPlayerRatio, lockcam_dbgEnemyRatio );
		CG_Printf( "%s", buf );
	}
}
