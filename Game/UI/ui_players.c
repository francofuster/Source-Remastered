/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
//
// ui_players.c

#include "ui_local.h"


#define UI_TIMER_GESTURE		2300
#define UI_TIMER_JUMP			1000
#define UI_TIMER_LAND			130
#define UI_TIMER_WEAPON_SWITCH	300
#define UI_TIMER_ATTACK			500
#define	UI_TIMER_MUZZLE_FLASH	20
#define	UI_TIMER_WEAPON_DELAY	250

#define JUMP_HEIGHT				56

#define SWINGSPEED				0.3f

#define SPIN_SPEED				0.9f
#define COAST_TIME				1000


static int			dp_realtime;
static float		jumpHeight;
static int			ui_loadTier;		// 0 = base form, see UI_PlayerInfo_SetModelTier


/*
===============
UI_PlayerInfo_SetWeapon
===============
*/
static void UI_PlayerInfo_SetWeapon( playerInfo_t *pi, weapon_t weaponNum ) {
	char		path[MAX_QPATH];

	pi->currentWeapon = weaponNum;
tryagain:
	pi->realWeapon = weaponNum;
	pi->weaponModel = 0;
	pi->barrelModel = 0;
	pi->flashModel = 0;

	if ( weaponNum == WP_NONE ) {
		return;
	}
	if( pi->weaponModel == 0 ) {
		if( weaponNum == WP_MACHINEGUN ) {
			weaponNum = WP_NONE;
			goto tryagain;
		}
		weaponNum = WP_MACHINEGUN;
		goto tryagain;
	}

	if ( weaponNum == WP_MACHINEGUN || weaponNum == WP_GAUNTLET || weaponNum == WP_BFG ) {
		COM_StripExtension( path, path, sizeof(path) );
		strcat( path, "_barrel.md3" );
		pi->barrelModel = trap_R_RegisterModel( path );
	}

	COM_StripExtension( path, path, sizeof(path) );
	strcat( path, "_flash.md3" );
	pi->flashModel = trap_R_RegisterModel( path );

	switch( weaponNum ) {
	case WP_GAUNTLET:
		MAKERGB( pi->flashDlightColor, 0.6f, 0.6f, 1 );
		break;

	case WP_MACHINEGUN:
		MAKERGB( pi->flashDlightColor, 1, 1, 0 );
		break;

	case WP_SHOTGUN:
		MAKERGB( pi->flashDlightColor, 1, 1, 0 );
		break;

	case WP_GRENADE_LAUNCHER:
		MAKERGB( pi->flashDlightColor, 1, 0.7f, 0.5f );
		break;

	case WP_ROCKET_LAUNCHER:
		MAKERGB( pi->flashDlightColor, 1, 0.75f, 0 );
		break;

	case WP_LIGHTNING:
		MAKERGB( pi->flashDlightColor, 0.6f, 0.6f, 1 );
		break;

	case WP_RAILGUN:
		MAKERGB( pi->flashDlightColor, 1, 0.5f, 0 );
		break;

	case WP_PLASMAGUN:
		MAKERGB( pi->flashDlightColor, 0.6f, 0.6f, 1 );
		break;

	case WP_BFG:
		MAKERGB( pi->flashDlightColor, 1, 0.7f, 1 );
		break;

	case WP_GRAPPLING_HOOK:
		MAKERGB( pi->flashDlightColor, 0.6f, 0.6f, 1 );
		break;

	default:
		MAKERGB( pi->flashDlightColor, 1, 1, 1 );
		break;
	}
}


/*
===============
UI_ForceLegsAnim
===============
*/
static void UI_ForceLegsAnim( playerInfo_t *pi, int anim ) {
	pi->legsAnim = ( ( pi->legsAnim & ANIM_TOGGLEBIT ) ^ ANIM_TOGGLEBIT ) | anim;

	if ( anim == ANIM_JUMP_UP ) {
		pi->legsAnimationTimer = UI_TIMER_JUMP;
	}
}


/*
===============
UI_SetLegsAnim
===============
*/
static void UI_SetLegsAnim( playerInfo_t *pi, int anim ) {
	if ( pi->pendingLegsAnim ) {
		anim = pi->pendingLegsAnim;
		pi->pendingLegsAnim = 0;
	}
	UI_ForceLegsAnim( pi, anim );
}


/*
===============
UI_ForceTorsoAnim
===============
*/
static void UI_ForceTorsoAnim( playerInfo_t *pi, int anim ) {
	pi->torsoAnim = ( ( pi->torsoAnim & ANIM_TOGGLEBIT ) ^ ANIM_TOGGLEBIT ) | anim;
/*
	if ( anim == ANIM_GESTURE ) {
		pi->torsoAnimationTimer = UI_TIMER_GESTURE;
	}

	if ( anim == ANIM_ATTACK || anim == ANIM_ATTACK2 ) {
		pi->torsoAnimationTimer = UI_TIMER_ATTACK;
	}
*/
}


/*
===============
UI_SetTorsoAnim
===============
*/
static void UI_SetTorsoAnim( playerInfo_t *pi, int anim ) {
	if ( pi->pendingTorsoAnim ) {
		anim = pi->pendingTorsoAnim;
		pi->pendingTorsoAnim = 0;
	}

	UI_ForceTorsoAnim( pi, anim );
}


/*
===============
UI_TorsoSequencing
===============
*/
static void UI_TorsoSequencing( playerInfo_t *pi ) {
	int		currentAnim;

	currentAnim = pi->torsoAnim & ~ANIM_TOGGLEBIT;
/*
	if ( pi->weapon != pi->currentWeapon ) {
		if ( currentAnim != ANIM_DROP ) {
			pi->torsoAnimationTimer = UI_TIMER_WEAPON_SWITCH;
			UI_ForceTorsoAnim( pi, ANIM_DROP );
		}
	}
*/
	if ( pi->torsoAnimationTimer > 0 ) {
		return;
	}
/*
	if( currentAnim == ANIM_GESTURE ) {
		UI_SetTorsoAnim( pi, ANIM_IDLE );
		return;
	}

	if( currentAnim == ANIM_ATTACK || currentAnim == ANIM_ATTACK2 ) {
		UI_SetTorsoAnim( pi, ANIM_IDLE );
		return;
	}

	if ( currentAnim == ANIM_DROP ) {
		UI_PlayerInfo_SetWeapon( pi, pi->weapon );
		pi->torsoAnimationTimer = UI_TIMER_WEAPON_SWITCH;
		UI_ForceTorsoAnim( pi, ANIM_RAISE );
		return;
	}

	if ( currentAnim == ANIM_RAISE ) {
		UI_SetTorsoAnim( pi, ANIM_IDLE );
		return;
	}
*/
}


/*
===============
UI_LegsSequencing
===============
*/
static void UI_LegsSequencing( playerInfo_t *pi ) {
	int		currentAnim;

	currentAnim = pi->legsAnim & ~ANIM_TOGGLEBIT;

	if ( pi->legsAnimationTimer > 0 ) {
		if ( currentAnim == ANIM_JUMP_UP ) {
			jumpHeight = JUMP_HEIGHT * sin( M_PI * ( UI_TIMER_JUMP - pi->legsAnimationTimer ) / UI_TIMER_JUMP );
		}
		return;
	}

	if ( currentAnim == ANIM_JUMP_UP ) {
		UI_ForceLegsAnim( pi, ANIM_LAND_UP );
		pi->legsAnimationTimer = UI_TIMER_LAND;
		jumpHeight = 0;
		return;
	}

	if ( currentAnim == ANIM_LAND_UP ) {
		UI_SetLegsAnim( pi, ANIM_IDLE );
		return;
	}
}


/*
======================
UI_PositionEntityOnTag
======================
*/
static void UI_PositionEntityOnTag( refEntity_t *entity, const refEntity_t *parent,
							clipHandle_t parentModel, char *tagName ) {
	int				i;
	orientation_t	lerped;

	// lerp the tag
	trap_CM_LerpTag( &lerped, parentModel, parent->oldframe, parent->frame,
		1.0 - parent->backlerp, tagName );

	// FIXME: allow origin offsets along tag?
	VectorCopy( parent->origin, entity->origin );
	for ( i = 0 ; i < 3 ; i++ ) {
		VectorMA( entity->origin, lerped.origin[i], parent->axis[i], entity->origin );
	}

	// cast away const because of compiler problems
	MatrixMultiply( lerped.axis, ((refEntity_t*)parent)->axis, entity->axis );
	entity->backlerp = parent->backlerp;
}


/*
======================
UI_PositionRotatedEntityOnTag
======================
*/
static void UI_PositionRotatedEntityOnTag( refEntity_t *entity, const refEntity_t *parent,
							clipHandle_t parentModel, char *tagName ) {
	int				i;
	orientation_t	lerped;
	vec3_t			tempAxis[3];

	// lerp the tag
	trap_CM_LerpTag( &lerped, parentModel, parent->oldframe, parent->frame,
		1.0 - parent->backlerp, tagName );

	// FIXME: allow origin offsets along tag?
	VectorCopy( parent->origin, entity->origin );
	for ( i = 0 ; i < 3 ; i++ ) {
		VectorMA( entity->origin, lerped.origin[i], parent->axis[i], entity->origin );
	}

	// cast away const because of compiler problems
	MatrixMultiply( entity->axis, ((refEntity_t *)parent)->axis, tempAxis );
	MatrixMultiply( lerped.axis, tempAxis, entity->axis );
}


/*
===============
UI_SetLerpFrameAnimation
===============
*/
static void UI_SetLerpFrameAnimation( playerInfo_t *ci, lerpFrame_t *lf, int newAnimation ) {
	animation_t	*anim;

	lf->animationNumber = newAnimation;
	newAnimation &= ~ANIM_TOGGLEBIT;

	if ( newAnimation < 0 || newAnimation >= MAX_ANIMATIONS ) {
		trap_Error( va("Bad animation number: %i", newAnimation) );
	}

	anim = &ci->animations[ newAnimation ];

	lf->animation = anim;
	lf->animationTime = lf->frameTime + anim->initialLerp;
}


/*
===============
UI_RunLerpFrame
===============
*/
static void UI_RunLerpFrame( playerInfo_t *ci, lerpFrame_t *lf, int newAnimation ) {
	int			f;
	animation_t	*anim;

	// see if the animation sequence is switching
	if ( newAnimation != lf->animationNumber || !lf->animation ) {
		UI_SetLerpFrameAnimation( ci, lf, newAnimation );
	}

	// if we have passed the current frame, move it to
	// oldFrame and calculate a new frame
	if ( dp_realtime >= lf->frameTime ) {
		lf->oldFrame = lf->frame;
		lf->oldFrameTime = lf->frameTime;

		// get the next frame based on the animation
		anim = lf->animation;
		if ( dp_realtime < lf->animationTime ) {
			lf->frameTime = lf->animationTime;		// initial lerp
		} else {
			lf->frameTime = lf->oldFrameTime + anim->frameLerp;
		}
		f = ( lf->frameTime - lf->animationTime ) / anim->frameLerp;
		if ( f >= anim->numFrames ) {
			f -= anim->numFrames;
			if ( anim->loopFrames ) {
				f %= anim->loopFrames;
				f += anim->numFrames - anim->loopFrames;
			} else {
				f = anim->numFrames - 1;
				// the animation is stuck at the end, so it
				// can immediately transition to another sequence
				lf->frameTime = dp_realtime;
			}
		}
		lf->frame = anim->firstFrame + f;
		if ( dp_realtime > lf->frameTime ) {
			lf->frameTime = dp_realtime;
		}
	}

	if ( lf->frameTime > dp_realtime + 200 ) {
		lf->frameTime = dp_realtime;
	}

	if ( lf->oldFrameTime > dp_realtime ) {
		lf->oldFrameTime = dp_realtime;
	}
	// calculate current lerp value
	if ( lf->frameTime == lf->oldFrameTime ) {
		lf->backlerp = 0;
	} else {
		lf->backlerp = 1.0 - (float)( dp_realtime - lf->oldFrameTime ) / ( lf->frameTime - lf->oldFrameTime );
	}
}


/*
===============
UI_PlayerAnimation
===============
*/
static void UI_PlayerAnimation( playerInfo_t *pi,
						int *legsOld, int *legs, float *legsBackLerp,
						int *torsoOld, int *torso, float *torsoBackLerp,
						int *headOld, int *head, float *headBackLerp) {

	// legs animation
	pi->legsAnimationTimer -= uis.frametime;
	if ( pi->legsAnimationTimer < 0 ) {
		pi->legsAnimationTimer = 0;
	}

	UI_LegsSequencing( pi );

	if ( pi->legs.yawing && ( pi->legsAnim & ~ANIM_TOGGLEBIT ) == ANIM_IDLE ) {
		UI_RunLerpFrame( pi, &pi->legs, ANIM_TURN );
	} else {
		UI_RunLerpFrame( pi, &pi->legs, pi->legsAnim );
	}
	*legsOld = pi->legs.oldFrame;
	*legs = pi->legs.frame;
	*legsBackLerp = pi->legs.backlerp;

	// torso animation
	pi->torsoAnimationTimer -= uis.frametime;
	if ( pi->torsoAnimationTimer < 0 ) {
		pi->torsoAnimationTimer = 0;
	}

	UI_TorsoSequencing( pi );

	UI_RunLerpFrame( pi, &pi->torso, pi->torsoAnim );
	*torsoOld = pi->torso.oldFrame;
	*torso = pi->torso.frame;
	*torsoBackLerp = pi->torso.backlerp;

	// ADDING FOR ZEQ2
	{
		int				torsoAnimNum;
		int				legsAnimNum;

		// NOTE: Torso animations take precedence over leg animations when deciding which
		//       head animation to play.
		torsoAnimNum = pi->torsoAnim & ~ANIM_TOGGLEBIT;
		if ( ANIM_FLY_UP == torsoAnimNum && pi->overrideHead ) {
			UI_RunLerpFrame( pi, &pi->head, ANIM_KI_CHARGE );
		} else if ( ANIM_FLY_DOWN == torsoAnimNum && pi->overrideHead ) {
			UI_RunLerpFrame( pi, &pi->head, ANIM_KI_CHARGE );
		} else if ( ANIM_FLOOR_RECOVER == torsoAnimNum ) {
			UI_RunLerpFrame( pi, &pi->head, ANIM_FLOOR_RECOVER );
		} else if ( ANIM_WALK == torsoAnimNum ) {
			UI_RunLerpFrame( pi, &pi->head, ANIM_WALK );
		} else if ( ANIM_RUN == torsoAnimNum ) {
			UI_RunLerpFrame( pi, &pi->head, ANIM_RUN );
		} else if ( ANIM_JUMP_UP == torsoAnimNum ) {
			UI_RunLerpFrame( pi, &pi->head, ANIM_JUMP_UP );
		} else if ( ANIM_LAND_UP == torsoAnimNum ) {
			UI_RunLerpFrame( pi, &pi->head, ANIM_LAND_UP );
		} else if ( ANIM_JUMP_FORWARD == torsoAnimNum ) {
			UI_RunLerpFrame( pi, &pi->head, ANIM_JUMP_FORWARD );
		} else if ( ANIM_LAND_FORWARD == torsoAnimNum ) {
			UI_RunLerpFrame( pi, &pi->head, ANIM_LAND_FORWARD );
		} else if ( ANIM_JUMP_BACK == torsoAnimNum ) {
			UI_RunLerpFrame( pi, &pi->head, ANIM_JUMP_BACK );
		} else if ( ANIM_LAND_BACK == torsoAnimNum ) {
			UI_RunLerpFrame( pi, &pi->head, ANIM_LAND_BACK );
		} else if ( ANIM_SWIM_IDLE == torsoAnimNum ) {
			UI_RunLerpFrame( pi, &pi->head, ANIM_SWIM_IDLE );
		} else if ( ANIM_SWIM == torsoAnimNum ) {
			UI_RunLerpFrame( pi, &pi->head, ANIM_SWIM );
		} else if ( ANIM_DASH_RIGHT == torsoAnimNum ) {
			UI_RunLerpFrame( pi, &pi->head, ANIM_DASH_RIGHT );
		} else if ( ANIM_DASH_LEFT == torsoAnimNum ) {
			UI_RunLerpFrame( pi, &pi->head, ANIM_DASH_LEFT );
		} else if ( ANIM_DASH_FORWARD == torsoAnimNum ) {
			UI_RunLerpFrame( pi, &pi->head, ANIM_DASH_FORWARD );
		} else if ( ANIM_DASH_BACKWARD == torsoAnimNum ) {
			UI_RunLerpFrame( pi, &pi->head, ANIM_DASH_BACKWARD );
		} else if ( ANIM_KI_CHARGE == torsoAnimNum ) {
			UI_RunLerpFrame( pi, &pi->head, ANIM_KI_CHARGE );
		} else if ( ANIM_PL_DOWN == torsoAnimNum ) {
			UI_RunLerpFrame( pi, &pi->head, ANIM_PL_DOWN );
		} else if ( ANIM_PL_UP == torsoAnimNum ) {
			UI_RunLerpFrame( pi, &pi->head, ANIM_PL_UP );
		} else if ( ANIM_TRANS_UP == torsoAnimNum ) {
			UI_RunLerpFrame( pi, &pi->head, ANIM_TRANS_UP );
		} else if ( ANIM_TRANS_BACK == torsoAnimNum ) {
			UI_RunLerpFrame( pi, &pi->head, ANIM_TRANS_BACK );
		} else if ( ANIM_FLY_IDLE == torsoAnimNum ) {
			UI_RunLerpFrame( pi, &pi->head, ANIM_FLY_IDLE );
		} else if ( ANIM_FLY_START == torsoAnimNum ) {
			UI_RunLerpFrame( pi, &pi->head, ANIM_FLY_START );
		} else if ( ANIM_FLY_FORWARD == torsoAnimNum ) {
			UI_RunLerpFrame( pi, &pi->head, ANIM_FLY_FORWARD );
		} else if ( ANIM_FLY_BACKWARD == torsoAnimNum ) {
			UI_RunLerpFrame( pi, &pi->head, ANIM_FLY_BACKWARD );
		} else if ( ANIM_FLY_UP == torsoAnimNum ) {
			UI_RunLerpFrame( pi, &pi->head, ANIM_FLY_UP );
		} else if ( ANIM_FLY_DOWN == torsoAnimNum ) {
			UI_RunLerpFrame( pi, &pi->head, ANIM_FLY_DOWN );
		} else if ( ANIM_STUNNED == torsoAnimNum ) {
			UI_RunLerpFrame( pi, &pi->head, ANIM_STUNNED );
		} else if ( ANIM_PUSH == torsoAnimNum ) {
			UI_RunLerpFrame( pi, &pi->head, ANIM_PUSH );
		} else if ( ANIM_DEFLECT == torsoAnimNum ) {
			UI_RunLerpFrame( pi, &pi->head, ANIM_DEFLECT );
		} else if ( ANIM_BLOCK == torsoAnimNum ) {
			UI_RunLerpFrame( pi, &pi->head, ANIM_BLOCK );
		} else if ( ANIM_SPEED_MELEE_ATTACK == torsoAnimNum ) {
			UI_RunLerpFrame( pi, &pi->head, ANIM_SPEED_MELEE_ATTACK );
		} else if ( ANIM_SPEED_MELEE_DODGE == torsoAnimNum ) {
			UI_RunLerpFrame( pi, &pi->head, ANIM_SPEED_MELEE_DODGE );
		} else if ( ANIM_SPEED_MELEE_BLOCK == torsoAnimNum ) {
			UI_RunLerpFrame( pi, &pi->head, ANIM_SPEED_MELEE_BLOCK );
		} else if ( ANIM_SPEED_MELEE_HIT == torsoAnimNum ) {
			UI_RunLerpFrame( pi, &pi->head, ANIM_SPEED_MELEE_HIT );
		} else if ( ANIM_BREAKER_MELEE_ATTACK1 == torsoAnimNum ) {
			UI_RunLerpFrame( pi, &pi->head, ANIM_BREAKER_MELEE_ATTACK1 );
		} else if ( ANIM_BREAKER_MELEE_ATTACK2 == torsoAnimNum ) {
			UI_RunLerpFrame( pi, &pi->head, ANIM_BREAKER_MELEE_ATTACK2 );
		} else if ( ANIM_BREAKER_MELEE_ATTACK3 == torsoAnimNum ) {
			UI_RunLerpFrame( pi, &pi->head, ANIM_BREAKER_MELEE_ATTACK3 );
		} else if ( ANIM_BREAKER_MELEE_ATTACK4 == torsoAnimNum ) {
			UI_RunLerpFrame( pi, &pi->head, ANIM_BREAKER_MELEE_ATTACK4 );
		} else if ( ANIM_BREAKER_MELEE_ATTACK5 == torsoAnimNum ) {
			UI_RunLerpFrame( pi, &pi->head, ANIM_BREAKER_MELEE_ATTACK5 );
		} else if ( ANIM_BREAKER_MELEE_ATTACK6 == torsoAnimNum ) {
			UI_RunLerpFrame( pi, &pi->head, ANIM_BREAKER_MELEE_ATTACK6 );
		} else if ( ANIM_BREAKER_MELEE_HIT1 == torsoAnimNum ) {
			UI_RunLerpFrame( pi, &pi->head, ANIM_BREAKER_MELEE_HIT1 );
		} else if ( ANIM_BREAKER_MELEE_HIT2 == torsoAnimNum ) {
			UI_RunLerpFrame( pi, &pi->head, ANIM_BREAKER_MELEE_HIT2 );
		} else if ( ANIM_BREAKER_MELEE_HIT3 == torsoAnimNum ) {
			UI_RunLerpFrame( pi, &pi->head, ANIM_BREAKER_MELEE_HIT3 );
		} else if ( ANIM_BREAKER_MELEE_HIT4 == torsoAnimNum ) {
			UI_RunLerpFrame( pi, &pi->head, ANIM_BREAKER_MELEE_HIT4 );
		} else if ( ANIM_BREAKER_MELEE_HIT5 == torsoAnimNum ) {
			UI_RunLerpFrame( pi, &pi->head, ANIM_BREAKER_MELEE_HIT5 );
		} else if ( ANIM_BREAKER_MELEE_HIT6 == torsoAnimNum ) {
			UI_RunLerpFrame( pi, &pi->head, ANIM_BREAKER_MELEE_HIT6 );
		} else if ( ANIM_POWER_MELEE_1_CHARGE == torsoAnimNum ) {
			UI_RunLerpFrame( pi, &pi->head, ANIM_POWER_MELEE_1_CHARGE );
		} else if ( ANIM_POWER_MELEE_2_CHARGE == torsoAnimNum ) {
			UI_RunLerpFrame( pi, &pi->head, ANIM_POWER_MELEE_2_CHARGE );
		} else if ( ANIM_POWER_MELEE_3_CHARGE == torsoAnimNum ) {
			UI_RunLerpFrame( pi, &pi->head, ANIM_POWER_MELEE_3_CHARGE );
		} else if ( ANIM_POWER_MELEE_4_CHARGE == torsoAnimNum ) {
			UI_RunLerpFrame( pi, &pi->head, ANIM_POWER_MELEE_4_CHARGE );
		} else if ( ANIM_POWER_MELEE_5_CHARGE == torsoAnimNum ) {
			UI_RunLerpFrame( pi, &pi->head, ANIM_POWER_MELEE_5_CHARGE );
		} else if ( ANIM_POWER_MELEE_6_CHARGE == torsoAnimNum ) {
			UI_RunLerpFrame( pi, &pi->head, ANIM_POWER_MELEE_6_CHARGE );
		} else if ( ANIM_POWER_MELEE_1_HIT == torsoAnimNum ) {
			UI_RunLerpFrame( pi, &pi->head, ANIM_POWER_MELEE_1_HIT );
		} else if ( ANIM_POWER_MELEE_2_HIT == torsoAnimNum ) {
			UI_RunLerpFrame( pi, &pi->head, ANIM_POWER_MELEE_2_HIT );
		} else if ( ANIM_POWER_MELEE_3_HIT == torsoAnimNum ) {
			UI_RunLerpFrame( pi, &pi->head, ANIM_POWER_MELEE_3_HIT );
		} else if ( ANIM_POWER_MELEE_4_HIT == torsoAnimNum ) {
			UI_RunLerpFrame( pi, &pi->head, ANIM_POWER_MELEE_4_HIT );
		} else if ( ANIM_POWER_MELEE_5_HIT == torsoAnimNum ) {
			UI_RunLerpFrame( pi, &pi->head, ANIM_POWER_MELEE_5_HIT );
		} else if ( ANIM_POWER_MELEE_6_HIT == torsoAnimNum ) {
			UI_RunLerpFrame( pi, &pi->head, ANIM_POWER_MELEE_6_HIT );
		} else if ( ANIM_STUNNED_MELEE == torsoAnimNum ) {
			UI_RunLerpFrame( pi, &pi->head, ANIM_STUNNED_MELEE );
		} else if ( ANIM_KNOCKBACK == torsoAnimNum ) {
			UI_RunLerpFrame( pi, &pi->head, ANIM_KNOCKBACK );
		} else if ( ANIM_KNOCKBACK_HIT_WALL == torsoAnimNum ) {
			UI_RunLerpFrame( pi, &pi->head, ANIM_KNOCKBACK_HIT_WALL );
		} else if ( ANIM_KNOCKBACK_RECOVER_1 == torsoAnimNum ) {
			UI_RunLerpFrame( pi, &pi->head, ANIM_KNOCKBACK_RECOVER_1 );
		} else if ( ANIM_KNOCKBACK_RECOVER_2 == torsoAnimNum ) {
			UI_RunLerpFrame( pi, &pi->head, ANIM_KNOCKBACK_RECOVER_2 );
		} else if ( ANIM_KI_ATTACK1_PREPARE <= torsoAnimNum && ANIM_KI_ATTACK6_ALT_FIRE >= torsoAnimNum ) {
			UI_RunLerpFrame( pi, &pi->head, torsoAnimNum - ANIM_KI_ATTACK1_PREPARE + ANIM_KI_ATTACK1_PREPARE );
		} else {
			legsAnimNum = pi->legsAnim & ~ANIM_TOGGLEBIT;
			if ( 0 ) {
			} else {
				UI_RunLerpFrame( pi, &pi->head, ANIM_IDLE );
			}
		}
	}

	*headOld = pi->head.oldFrame;
	*head = pi->head.frame;
	*headBackLerp = pi->head.backlerp;

	// END ADDING
}


/*
==================
UI_SwingAngles
==================
*/
static void UI_SwingAngles( float destination, float swingTolerance, float clampTolerance,
					float speed, float *angle, qboolean *swinging ) {
	float	swing;
	float	move;
	float	scale;

	if ( !*swinging ) {
		// see if a swing should be started
		swing = AngleSubtract( *angle, destination );
		if ( swing > swingTolerance || swing < -swingTolerance ) {
			*swinging = qtrue;
		}
	}

	if ( !*swinging ) {
		return;
	}

	// modify the speed depending on the delta
	// so it doesn't seem so linear
	swing = AngleSubtract( destination, *angle );
	scale = fabs( swing );
	if ( scale < swingTolerance * 0.5 ) {
		scale = 0.5;
	} else if ( scale < swingTolerance ) {
		scale = 1.0;
	} else {
		scale = 2.0;
	}

	// swing towards the destination angle
	if ( swing >= 0 ) {
		move = uis.frametime * scale * speed;
		if ( move >= swing ) {
			move = swing;
			*swinging = qfalse;
		}
		*angle = AngleMod( *angle + move );
	} else if ( swing < 0 ) {
		move = uis.frametime * scale * -speed;
		if ( move <= swing ) {
			move = swing;
			*swinging = qfalse;
		}
		*angle = AngleMod( *angle + move );
	}

	// clamp to no more than tolerance
	swing = AngleSubtract( destination, *angle );
	if ( swing > clampTolerance ) {
		*angle = AngleMod( destination - (clampTolerance - 1) );
	} else if ( swing < -clampTolerance ) {
		*angle = AngleMod( destination + (clampTolerance - 1) );
	}
}


/*
======================
UI_MovedirAdjustment
======================
*/
static float UI_MovedirAdjustment( playerInfo_t *pi ) {
	vec3_t		relativeAngles;
	vec3_t		moveVector;

	VectorSubtract( pi->viewAngles, pi->moveAngles, relativeAngles );
	AngleVectors( relativeAngles, moveVector, NULL, NULL );
	if ( Q_fabs( moveVector[0] ) < 0.01 ) {
		moveVector[0] = 0.0;
	}
	if ( Q_fabs( moveVector[1] ) < 0.01 ) {
		moveVector[1] = 0.0;
	}

	if ( moveVector[1] == 0 && moveVector[0] > 0 ) {
		return 0;
	}
	if ( moveVector[1] < 0 && moveVector[0] > 0 ) {
		return 22;
	}
	if ( moveVector[1] < 0 && moveVector[0] == 0 ) {
		return 45;
	}
	if ( moveVector[1] < 0 && moveVector[0] < 0 ) {
		return -22;
	}
	if ( moveVector[1] == 0 && moveVector[0] < 0 ) {
		return 0;
	}
	if ( moveVector[1] > 0 && moveVector[0] < 0 ) {
		return 22;
	}
	if ( moveVector[1] > 0 && moveVector[0] == 0 ) {
		return  -45;
	}

	return -22;
}


/*
===============
UI_PlayerAngles
===============
*/
static void UI_PlayerAngles( playerInfo_t *pi, vec3_t legs[3], vec3_t torso[3], vec3_t head[3] ) {
	vec3_t		legsAngles, torsoAngles, headAngles;
	float		dest;
	float		adjust;

	VectorCopy( pi->viewAngles, headAngles );
	headAngles[YAW] = AngleMod( headAngles[YAW] );
	VectorClear( legsAngles );
	VectorClear( torsoAngles );

	// --------- yaw -------------

	// allow yaw to drift a bit
	if ( ( pi->legsAnim & ~ANIM_TOGGLEBIT ) != ANIM_IDLE
		|| ( pi->torsoAnim & ~ANIM_TOGGLEBIT ) != ANIM_IDLE  ) {
		// if not standing still, always point all in the same direction
		pi->torso.yawing = qtrue;	// always center
		pi->torso.pitching = qtrue;	// always center
		pi->legs.yawing = qtrue;	// always center
	}

	// adjust legs for movement dir
	adjust = UI_MovedirAdjustment( pi );
	legsAngles[YAW] = headAngles[YAW] + adjust;
	torsoAngles[YAW] = headAngles[YAW] + 0.25 * adjust;


	// torso
	UI_SwingAngles( torsoAngles[YAW], 25, 90, SWINGSPEED, &pi->torso.yawAngle, &pi->torso.yawing );
	UI_SwingAngles( legsAngles[YAW], 40, 90, SWINGSPEED, &pi->legs.yawAngle, &pi->legs.yawing );

	torsoAngles[YAW] = pi->torso.yawAngle;
	legsAngles[YAW] = pi->legs.yawAngle;

	// --------- pitch -------------

	// only show a fraction of the pitch angle in the torso
	if ( headAngles[PITCH] > 180 ) {
		dest = (-360 + headAngles[PITCH]) * 0.75;
	} else {
		dest = headAngles[PITCH] * 0.75;
	}
	UI_SwingAngles( dest, 15, 30, 0.1f, &pi->torso.pitchAngle, &pi->torso.pitching );
	torsoAngles[PITCH] = pi->torso.pitchAngle;

	// torso rotation locked?
//	if ( pi->fixedtorso ) {
		torsoAngles[PITCH] = 0.0f;
//	}

//	if ( pi->fixedlegs ) {
		legsAngles[YAW] = torsoAngles[YAW];
		legsAngles[PITCH] = 0.0f;
		legsAngles[ROLL] = 0.0f;
//	}
	// pull the angles back out of the hierarchial chain
	AnglesSubtract( headAngles, torsoAngles, headAngles );
	AnglesSubtract( torsoAngles, legsAngles, torsoAngles );
	AnglesToAxis( legsAngles, legs );
	AnglesToAxis( torsoAngles, torso );
	AnglesToAxis( headAngles, head );
}


/*
===============
UI_PlayerFloatSprite
===============
*/
static void UI_PlayerFloatSprite( playerInfo_t *pi, vec3_t origin, qhandle_t shader ) {
	refEntity_t		ent;

	memset( &ent, 0, sizeof( ent ) );
	VectorCopy( origin, ent.origin );
	ent.origin[2] += 48;
	ent.reType = RT_SPRITE;
	ent.customShader = shader;
	ent.radius = 10;
	ent.renderfx = 0;
	trap_R_AddRefEntityToScene( &ent );
}


/*
======================
UI_MachinegunSpinAngle
======================
*/
float	UI_MachinegunSpinAngle( playerInfo_t *pi ) {
	int		delta;
	float	angle;
	float	speed;
	int		torsoAnim;
/*
	delta = dp_realtime - pi->barrelTime;
	if ( pi->barrelSpinning ) {
		angle = pi->barrelAngle + delta * SPIN_SPEED;
	} else {
		if ( delta > COAST_TIME ) {
			delta = COAST_TIME;
		}

		speed = 0.5 * ( SPIN_SPEED + (float)( COAST_TIME - delta ) / COAST_TIME );
		angle = pi->barrelAngle + delta * speed;
	}

	torsoAnim = pi->torsoAnim  & ~ANIM_TOGGLEBIT;

	if( torsoAnim == ANIM_ATTACK2 ) {
		torsoAnim = ANIM_ATTACK;
	}
	if ( pi->barrelSpinning == !(torsoAnim == ANIM_ATTACK) ) {
		pi->barrelTime = dp_realtime;
		pi->barrelAngle = AngleMod( angle );
		pi->barrelSpinning = !!(torsoAnim == ANIM_ATTACK);
	}
*/
	return angle;
}



/*
=======================================================================

TRANSFORMATION AURA

The convex hull aura of cg_auras.c, rebuilt for the menu preview: the
tag_aura points of the three models are projected on the screen, their
convex hull gets spikes along its edge and a tail points up. Lightning is
the boltEffect sprite flickering around the fighter, as in the game.

=======================================================================
*/

#define UI_AURA_MAXPOINTS		49		// 16 tags per model and the tail
#define UI_AURA_SPIKES			50
#define UI_AURA_FLATTEN			0.75f
#define UI_AURA_ROOTCUT_FRAC	0.80f
#define UI_AURA_ROOTCUT_DIST	7.00f
#define UI_BOLT_LIFE			250
#define UI_AURA_SIZE			0.55f	// the preview is seen up close, the spikes of the game would fill the frame

typedef struct {
	vec3_t		pos_world;
	vec2_t		pos_screen;
	vec3_t		normal;
	float		length;
} uiAuraTag_t;

static uiAuraTag_t	auraHull[UI_AURA_MAXPOINTS];
static int			auraHullCount;
static float		auraCircumference;
static vec3_t		auraOrigin;
static vec3_t		auraRoot;
static vec3_t		auraTail;
static float		auraTanX;
static float		auraTanY;

/*
===============
UI_ParseAura

the aura keys of tierDefault.cfg and tier.cfg
===============
*/
static void UI_ParseAura( uiAura_t *aura, qboolean *exists, qboolean *always, const char *filename ) {
	static char		text[8192];
	fileHandle_t	f;
	int				len;
	int				i;
	char			*p;
	char			*token;

	len = trap_FS_FOpenFile( filename, &f, FS_READ );
	if ( len <= 0 ) {
		return;
	}
	if ( len >= (int)sizeof( text ) ) {
		len = sizeof( text ) - 1;
	}
	trap_FS_Read( text, len, f );
	text[len] = 0;
	trap_FS_FCloseFile( f );

	p = text;
	while ( 1 ) {
		token = COM_Parse( &p );
		if ( !token[0] ) {
			break;
		}
		if ( !Q_stricmp( token, "auraExists" ) ) {
			*exists = ( strlen( COM_Parse( &p ) ) == 4 );
		}
		else if ( !Q_stricmp( token, "auraAlways" ) ) {
			*always = ( strlen( COM_Parse( &p ) ) == 4 );
		}
		else if ( !Q_stricmp( token, "showLightning" ) ) {
			aura->lightning = ( strlen( COM_Parse( &p ) ) == 4 );
		}
		else if ( !Q_stricmp( token, "auraColor" ) ) {
			for ( i = 0; i < 3; i++ ) {
				aura->color[i] = atof( COM_Parse( &p ) );
			}
		}
		else if ( !Q_stricmp( token, "auraLightColor" ) ) {
			for ( i = 0; i < 3; i++ ) {
				aura->lightColor[i] = atof( COM_Parse( &p ) );
			}
		}
		else if ( !Q_stricmp( token, "auraScale" ) ) {
			aura->scale = atof( COM_Parse( &p ) );
		}
		else if ( !Q_stricmp( token, "auraLength" ) ) {
			aura->length = atof( COM_Parse( &p ) );
		}
		else if ( !Q_stricmp( token, "auraTagCount" ) ) {
			for ( i = 0; i < 3; i++ ) {
				aura->numTags[i] = atoi( COM_Parse( &p ) );
				if ( aura->numTags[i] > 16 ) {
					aura->numTags[i] = 16;
				}
			}
		}
		else if ( !Q_stricmp( token, "auraShader" ) ) {
			aura->shader = trap_R_RegisterShaderNoMip( COM_Parse( &p ) );
		}
		else if ( !Q_stricmp( token, "lightningShader" ) ) {
			aura->lightningShader = trap_R_RegisterShaderNoMip( COM_Parse( &p ) );
		}
	}
}

/*
===============
UI_LoadAura

transformations show their aura (always on in the preview), the base form
only when its tier.cfg says auraAlways
===============
*/
static void UI_LoadAura( uiAura_t *aura, const char *modelName, int tier ) {
	qboolean	exists;
	qboolean	always;

	memset( aura, 0, sizeof( *aura ) );
	exists = qtrue;
	always = qfalse;
	aura->scale = 1.5f;
	aura->length = 32.0f;
	UI_ParseAura( aura, &exists, &always, "players/tierDefault.cfg" );
	UI_ParseAura( aura, &exists, &always, va( "players/%s/tier%i/tier.cfg", modelName, tier + 1 ) );
	aura->show = exists && aura->shader && ( always || tier > 0 );
	if ( aura->lightning && !aura->lightningShader ) {
		aura->lightningShader = trap_R_RegisterShaderNoMip( "boltEffect" );
	}
	aura->startTime = uis.realtime;
}

// same projection as CG_WorldCoordToScreenCoordVec, for a view at the origin looking down +x
static qboolean UI_Aura_Project( const vec3_t p, vec2_t out ) {
	if ( p[0] < 0.01f ) {
		return qfalse;
	}
	out[0] = 320.0f - 320.0f / p[0] / auraTanX * p[1];
	out[1] = 240.0f - 240.0f / p[0] / auraTanY * p[2];
	return qtrue;
}

static void UI_Aura_AddTags( const refEntity_t *ent, qhandle_t model, int count ) {
	orientation_t	lerped;
	char			tagName[16];
	uiAuraTag_t		*t;
	int				i;
	int				j;

	for ( i = 0; i < count && auraHullCount < UI_AURA_MAXPOINTS - 1; i++ ) {
		Com_sprintf( tagName, sizeof( tagName ), "tag_aura%i", i );
		// the UI syscall always returns 0; a missing tag comes back at the origin
		trap_CM_LerpTag( &lerped, model, ent->oldframe, ent->frame, 1.0f - ent->backlerp, tagName );
		if ( !lerped.origin[0] && !lerped.origin[1] && !lerped.origin[2] ) {
			continue;
		}
		t = &auraHull[auraHullCount];
		VectorCopy( ent->origin, t->pos_world );
		for ( j = 0; j < 3; j++ ) {
			VectorMA( t->pos_world, lerped.origin[j], ent->axis[j], t->pos_world );
		}
		if ( UI_Aura_Project( t->pos_world, t->pos_screen ) ) {
			auraHullCount++;
		}
	}
}

// the hull point furthest down the tail axis is where the aura opens up
static void UI_Aura_MarkRoot( void ) {
	vec3_t	prj;
	vec3_t	tailDir;
	float	maxDist;
	float	dist;
	int		i;

	maxDist = 0;
	VectorCopy( auraOrigin, auraRoot );
	for ( i = 0; i < auraHullCount; i++ ) {
		ProjectPointOnLine( auraHull[i].pos_world, auraOrigin, auraTail, prj );
		dist = Distance( prj, auraTail );
		if ( dist > maxDist ) {
			maxDist = dist;
			VectorCopy( prj, auraRoot );
		}
	}
	VectorSubtract( auraTail, auraOrigin, tailDir );
	VectorNormalize( tailDir );
	VectorMA( auraRoot, -4.8f, tailDir, auraRoot );
}

static void UI_Aura_SortAngle( uiAuraTag_t *points, float *angles, int lowbound, int highbound ) {
	int			low, high;
	float		mid;
	uiAuraTag_t	tempPoint;
	float		tempAngle;

	low = lowbound;
	high = highbound;
	mid = angles[( low + high ) / 2];
	do {
		while ( angles[low] < mid ) low++;
		while ( angles[high] > mid ) high--;
		if ( low <= high ) {
			tempPoint = points[low];
			points[low] = points[high];
			points[high] = tempPoint;
			tempAngle = angles[low];
			angles[low] = angles[high];
			angles[high] = tempAngle;
			low++;
			high--;
		}
	} while ( !( low > high ) );
	if ( high > lowbound ) UI_Aura_SortAngle( points, angles, lowbound, high );
	if ( low < highbound ) UI_Aura_SortAngle( points, angles, low, highbound );
}

// Graham scan on the screen positions, as CG_Aura_ArrangeConvexHull
static qboolean UI_Aura_ArrangeHull( uiAuraTag_t *points, int *nr_points ) {
	static float		angles[UI_AURA_MAXPOINTS];
	static uiAuraTag_t	buffer[UI_AURA_MAXPOINTS];
	int					amount, index, pivotIndex;
	uiAuraTag_t			pivot, behind, infront;
	qboolean			rightTurn;
	float				dx, dy;

	amount = *nr_points;
	if ( amount == 3 ) {
		return qtrue;
	}
	if ( amount < 3 ) {
		return qfalse;
	}

	pivotIndex = 0;
	for ( index = 1; index < amount; index++ ) {
		if ( points[index].pos_screen[1] < points[pivotIndex].pos_screen[1] ) {
			pivotIndex = index;
		}
		else if ( points[index].pos_screen[1] == points[pivotIndex].pos_screen[1] &&
			points[index].pos_screen[0] > points[pivotIndex].pos_screen[0] ) {
			pivotIndex = index;
		}
	}
	pivot = points[pivotIndex];
	points[pivotIndex] = points[amount - 1];
	amount--;

	for ( index = 0; index < amount; index++ ) {
		dx = pivot.pos_screen[0] - points[index].pos_screen[0];
		dy = pivot.pos_screen[1] - points[index].pos_screen[1];
		angles[index] = ( dx || dy ) ? dx / Q_hypot( dx, dy ) : 0.0f;
	}
	UI_Aura_SortAngle( points, angles, 0, amount - 1 );

	index = 1;
	do {
		if ( index == 0 ) {
			rightTurn = qtrue;
		}
		else {
			behind = points[index - 1];
			infront = ( index == amount - 1 ) ? pivot : points[index + 1];
			rightTurn = ( ( behind.pos_screen[0] - points[index].pos_screen[0] ) * ( infront.pos_screen[1] - points[index].pos_screen[1] ) -
				( infront.pos_screen[0] - points[index].pos_screen[0] ) * ( behind.pos_screen[1] - points[index].pos_screen[1] ) ) < 0;
		}
		if ( rightTurn ) {
			index++;
		}
		else {
			if ( index == amount - 1 ) {
				amount--;
			}
			else {
				memcpy( buffer, &points[index + 1], sizeof( uiAuraTag_t ) * ( amount - index - 1 ) );
				memcpy( &points[index], buffer, sizeof( uiAuraTag_t ) * ( amount - index - 1 ) );
				amount--;
			}
			index--;
		}
	} while ( index < amount - 1 );

	points[amount] = pivot;
	amount++;
	*nr_points = amount;
	return qtrue;
}

// segment lengths and outward normals, the view is at the origin
static void UI_Aura_SetHullAttributes( void ) {
	int		index, behind, infront;
	vec3_t	lineBehind, lineInfront, viewLine;
	vec3_t	tempBehind, tempInfront;

	auraCircumference = 0.0f;
	for ( index = 0; index < auraHullCount; index++ ) {
		behind = ( index == 0 ) ? auraHullCount - 1 : index - 1;
		infront = ( index == auraHullCount - 1 ) ? 0 : index + 1;

		VectorCopy( auraHull[index].pos_world, viewLine );
		VectorSubtract( auraHull[index].pos_world, auraHull[behind].pos_world, lineBehind );
		VectorSubtract( auraHull[infront].pos_world, auraHull[index].pos_world, lineInfront );
		VectorNormalize( lineBehind );
		auraCircumference += ( auraHull[index].length = VectorNormalize( lineInfront ) );

		CrossProduct( lineBehind, viewLine, tempBehind );
		CrossProduct( lineInfront, viewLine, tempInfront );
		VectorNormalize( tempBehind );
		VectorNormalize( tempInfront );
		VectorAdd( tempBehind, tempInfront, auraHull[index].normal );
		VectorNormalize( auraHull[index].normal );
	}
}

static void UI_Aura_DrawSpike( vec3_t start, vec3_t end, float width, qhandle_t shader, vec4_t rgba ) {
	vec3_t		line, offset;
	polyVert_t	verts[4];
	int			i, j;

	VectorSubtract( end, start, line );
	CrossProduct( start, line, offset );
	if ( !VectorNormalize( offset ) ) {
		return;
	}
	VectorMA( end, -width, offset, verts[0].xyz );
	verts[0].st[0] = 1; verts[0].st[1] = 0;
	VectorMA( end, width, offset, verts[1].xyz );
	verts[1].st[0] = 0; verts[1].st[1] = 0;
	VectorMA( start, width, offset, verts[2].xyz );
	verts[2].st[0] = 0; verts[2].st[1] = 1;
	VectorMA( start, -width, offset, verts[3].xyz );
	verts[3].st[0] = 1; verts[3].st[1] = 1;
	for ( i = 0; i < 4; i++ ) {
		for ( j = 0; j < 4; j++ ) {
			verts[i].modulate[j] = 255 * rgba[j];
		}
	}
	trap_R_AddPolyToScene( shader, 4, verts );
}

static void UI_Aura_Spike( uiAura_t *aura, int spikeNr, float alphaModulate ) {
	int		start, end;
	float	progress, lengthPos, lengthSoFar;
	vec3_t	lerpPos, lerpNormal, lerpDir, endPos, basePos;
	vec3_t	edge, tailDir, tempVec, viewLine;
	float	lerpTime, lerpModulate, lerpBorder, baseBorder, lerpSize;
	vec4_t	color;

	if ( !( spikeNr % 3 ) ) {
		lerpTime = ( uis.realtime % 400 ) / 400.0f;
		lerpSize = 14.0f * ( 1 + lerpTime );
		lerpBorder = 2.5f * ( 1.75f + lerpTime );
		baseBorder = 2.5f * 1.75f;
	}
	else if ( !( spikeNr % 2 ) ) {
		lerpTime = ( ( uis.realtime + 200 ) % 400 ) / 400.0f;
		lerpSize = 12.0f * ( 1 + lerpTime );
		lerpBorder = 3.0f * ( 1.75f + lerpTime );
		baseBorder = 3.0f * 1.75f;
	}
	else {
		lerpTime = ( uis.realtime % 500 ) / 500.0f;
		lerpSize = 10.0f * ( 1 + lerpTime );
		lerpBorder = 2.75f * ( 1.75f + lerpTime );
		baseBorder = 2.75f * 1.75f;
	}
	lerpModulate = -4 * lerpTime * lerpTime + 4 * lerpTime;
	lerpSize *= UI_AURA_SIZE;
	lerpBorder *= UI_AURA_SIZE;
	baseBorder *= UI_AURA_SIZE;
	color[0] = aura->color[0];
	color[1] = aura->color[1];
	color[2] = aura->color[2];
	color[3] = lerpModulate * alphaModulate;

	// position along the hull
	lengthPos = auraCircumference * ( (float)spikeNr / (float)( UI_AURA_SPIKES - 1 ) );
	lengthSoFar = 0;
	for ( start = 0; start < auraHullCount - 1 && lengthSoFar + auraHull[start].length < lengthPos; start++ ) {
		lengthSoFar += auraHull[start].length;
	}
	end = ( start + 1 == auraHullCount ) ? 0 : start + 1;
	progress = auraHull[start].length > 0.0f ? ( lengthPos - lengthSoFar ) / auraHull[start].length : 0.0f;

	VectorClear( lerpNormal );
	VectorMA( lerpNormal, 1.0f - progress, auraHull[start].normal, lerpNormal );
	VectorMA( lerpNormal, progress, auraHull[end].normal, lerpNormal );
	VectorNormalize( lerpNormal );

	VectorSubtract( auraHull[end].pos_world, auraHull[start].pos_world, edge );
	VectorMA( auraHull[start].pos_world, progress, edge, lerpPos );
	VectorMA( lerpPos, baseBorder, lerpNormal, basePos );
	VectorMA( lerpPos, lerpBorder, lerpNormal, lerpPos );

	// flatten the direction so it does not point at the tip too much
	VectorSubtract( lerpPos, auraRoot, lerpDir );
	VectorSubtract( auraTail, auraOrigin, tailDir );
	VectorPllComponent( lerpDir, tailDir, edge );
	VectorScale( edge, UI_AURA_FLATTEN, edge );
	VectorSubtract( lerpDir, edge, lerpDir );
	VectorNormalize( lerpDir );

	// skip spikes running into the screen near the root
	VectorCopy( lerpPos, viewLine );
	VectorNormalize( viewLine );
	CrossProduct( viewLine, lerpDir, tempVec );
	if ( VectorLength( tempVec ) < UI_AURA_ROOTCUT_FRAC && Distance( basePos, auraRoot ) < UI_AURA_ROOTCUT_DIST ) {
		return;
	}

	VectorMA( lerpPos, lerpBorder, lerpDir, lerpPos );
	VectorMA( lerpPos, lerpSize, lerpDir, endPos );
	UI_Aura_DrawSpike( lerpPos, endPos, lerpSize / 1.25f, aura->shader, color );
}

/*
===============
UI_AddAura
===============
*/
static void UI_AddAura( playerInfo_t *pi, const refdef_t *refdef, const refEntity_t *legs, const refEntity_t *torso, const refEntity_t *head ) {
	uiAura_t	*aura;
	float		fade;
	int			i;

	aura = &pi->aura;
	if ( !aura->show ) {
		return;
	}
	fade = ( uis.realtime - aura->startTime ) / 400.0f;
	if ( fade > 1.0f ) {
		fade = 1.0f;
	}
	if ( fade <= 0.0f ) {
		return;
	}

	auraTanX = tan( refdef->fov_x / 360.0f * M_PI );
	auraTanY = tan( refdef->fov_y / 360.0f * M_PI );
	VectorCopy( legs->origin, auraOrigin );
	VectorCopy( auraOrigin, auraTail );
	auraTail[2] += aura->scale * aura->length;

	auraHullCount = 0;
	UI_Aura_AddTags( head, pi->headModel, aura->numTags[0] );
	UI_Aura_AddTags( torso, pi->torsoModel, aura->numTags[1] );
	UI_Aura_AddTags( legs, pi->legsModel, aura->numTags[2] );
	VectorCopy( auraTail, auraHull[auraHullCount].pos_world );
	if ( UI_Aura_Project( auraHull[auraHullCount].pos_world, auraHull[auraHullCount].pos_screen ) ) {
		auraHullCount++;
	}
	UI_Aura_MarkRoot();
	if ( !UI_Aura_ArrangeHull( auraHull, &auraHullCount ) ) {
		return;
	}
	UI_Aura_SetHullAttributes();
	if ( auraCircumference <= 0.0f ) {
		return;
	}
	for ( i = 0; i < UI_AURA_SPIKES; i++ ) {
		UI_Aura_Spike( aura, i, fade );
	}

}

/*
===============
UI_AddLightning

short bolt sprites around the fighter, spawned every few hundred ms and
fading out like the LE_FADE_ALPHA ones of CG_LightningEffect
===============
*/
static void UI_AddLightning( playerInfo_t *pi, const vec3_t origin ) {
	uiAura_t	*aura;
	refEntity_t	ent;
	int			i;
	int			slot;
	int			age;

	aura = &pi->aura;
	if ( !aura->lightning || !aura->lightningShader ) {
		return;
	}

	if ( uis.realtime >= aura->nextBolt ) {
		slot = 0;
		for ( i = 1; i < UI_AURA_BOLTS; i++ ) {
			if ( aura->boltTime[i] < aura->boltTime[slot] ) {
				slot = i;
			}
		}
		aura->boltTime[slot] = uis.realtime;
		aura->boltOffset[slot][0] = ( random() * 24 + 8 ) - ( random() * 24 + 8 );
		aura->boltOffset[slot][1] = ( random() * 24 + 8 ) - ( random() * 24 + 8 );
		aura->boltOffset[slot][2] = ( random() * 32 + 16 ) - random() * 40;
		aura->nextBolt = uis.realtime + 120 + random() * 380;
	}

	for ( i = 0; i < UI_AURA_BOLTS; i++ ) {
		age = uis.realtime - aura->boltTime[i];
		if ( !aura->boltTime[i] || age >= UI_BOLT_LIFE ) {
			continue;
		}
		memset( &ent, 0, sizeof( ent ) );
		ent.reType = RT_SPRITE;
		ent.customShader = aura->lightningShader;
		ent.radius = 16;
		ent.shaderRGBA[0] = ent.shaderRGBA[1] = ent.shaderRGBA[2] = 255;
		ent.shaderRGBA[3] = 255 * ( 1.0f - (float)age / UI_BOLT_LIFE );
		VectorAdd( origin, aura->boltOffset[i], ent.origin );
		AxisClear( ent.axis );
		trap_R_AddRefEntityToScene( &ent );
	}
}

/*
===============
UI_DrawPlayer
===============
*/
void UI_DrawPlayer( float x, float y, float w, float h, playerInfo_t *pi, int time ) {
	refdef_t		refdef;
	refEntity_t		legs;
	refEntity_t		torso;
	refEntity_t		head;
	refEntity_t		gun;
	refEntity_t		barrel;
	refEntity_t		flash;
	vec3_t			origin;
	int				renderfx;
	vec3_t			mins = {-16, -16, -24};
	vec3_t			maxs = {16, 16, 32};
	float			len;
	float			xx;

	if(pi->legsModel && !pi->torsoModel && !pi->headModel)
	{
		UI_DrawPlayer_zMesh(x,y,w,h,pi,time);
		return;
	}
	if ( !pi->legsModel || !pi->torsoModel || !pi->headModel || !pi->animations[0].numFrames ) {
		return;
	}

	dp_realtime = time;

	if ( pi->pendingWeapon != -1 && dp_realtime > pi->weaponTimer ) {
		pi->weapon = pi->pendingWeapon;
		pi->lastWeapon = pi->pendingWeapon;
		pi->pendingWeapon = -1;
		pi->weaponTimer = 0;
	}

	UI_AdjustFrom640( &x, &y, &w, &h );

	y -= jumpHeight;

	memset( &refdef, 0, sizeof( refdef ) );
	memset( &legs, 0, sizeof(legs) );
	memset( &torso, 0, sizeof(torso) );
	memset( &head, 0, sizeof(head) );

	refdef.rdflags = RDF_NOWORLDMODEL;

	AxisClear( refdef.viewaxis );

	refdef.x = x;
	refdef.y = y;
	refdef.width = w;
	refdef.height = h;

	refdef.fov_x = (int)((float)refdef.width / 640.0f * 35.0f);
	xx = refdef.width / tan( refdef.fov_x / 360 * M_PI );
	refdef.fov_y = atan2( refdef.height, xx );
	refdef.fov_y *= ( 360 / M_PI );

	// calculate distance so the player nearly fills the box
	len = 0.6 * ( maxs[2] - mins[2] );
	origin[0] = len / tan( DEG2RAD(refdef.fov_x) * 0.5 );
	origin[1] = 0.5 * ( mins[1] + maxs[1] );
	origin[2] = -0.5 * ( mins[2] + maxs[2] );

	refdef.time = dp_realtime;

	trap_R_ClearScene();

	// get the rotation information
	UI_PlayerAngles( pi, legs.axis, torso.axis, head.axis );

	// get the animation state (after rotation, to allow feet shuffle)
	UI_PlayerAnimation( pi, &legs.oldframe, &legs.frame, &legs.backlerp,
		 &torso.oldframe, &torso.frame, &torso.backlerp,
		 &head.oldframe, &head.frame, &head.backlerp);

	renderfx = RF_LIGHTING_ORIGIN | RF_NOSHADOW;

	//
	// add the legs
	//
	legs.hModel = pi->legsModel;
	legs.customSkin = pi->legsSkin;

	VectorCopy( origin, legs.origin );

	VectorCopy( origin, legs.lightingOrigin );
	legs.renderfx = renderfx;
	VectorCopy (legs.origin, legs.oldorigin);

	trap_R_AddRefEntityToScene( &legs );

	if (!legs.hModel) {
		return;
	}

	//
	// add the torso
	//
	torso.hModel = pi->torsoModel;
	if (!torso.hModel) {
		return;
	}

	torso.customSkin = pi->torsoSkin;

	VectorCopy( origin, torso.lightingOrigin );

	UI_PositionRotatedEntityOnTag( &torso, &legs, pi->legsModel, "tag_torso");

	torso.renderfx = renderfx;

	trap_R_AddRefEntityToScene( &torso );

	//
	// add the head
	//
	head.hModel = pi->headModel;
	if (!head.hModel) {
		return;
	}
	head.customSkin = pi->headSkin;

	VectorCopy( origin, head.lightingOrigin );

	UI_PositionRotatedEntityOnTag( &head, &torso, pi->torsoModel, "tag_head");

	head.renderfx = renderfx;

	trap_R_AddRefEntityToScene( &head );

	// transformation preview
	UI_AddAura( pi, &refdef, &legs, &torso, &head );
	UI_AddLightning( pi, legs.origin );
/*
	//
	// add the gun
	//
	if ( pi->currentWeapon != WP_NONE ) {
		memset( &gun, 0, sizeof(gun) );
		gun.hModel = pi->weaponModel;
		VectorCopy( origin, gun.lightingOrigin );
		UI_PositionEntityOnTag( &gun, &torso, pi->torsoModel, "tag_weapon");
		gun.renderfx = renderfx;
		trap_R_AddRefEntityToScene( &gun );
	}

	//
	// add the spinning barrel
	//
	if ( pi->realWeapon == WP_MACHINEGUN || pi->realWeapon == WP_GAUNTLET || pi->realWeapon == WP_BFG ) {
		vec3_t	angles;

		memset( &barrel, 0, sizeof(barrel) );
		VectorCopy( origin, barrel.lightingOrigin );
		barrel.renderfx = renderfx;

		barrel.hModel = pi->barrelModel;
		angles[YAW] = 0;
		angles[PITCH] = 0;
		angles[ROLL] = UI_MachinegunSpinAngle( pi );
		if( pi->realWeapon == WP_GAUNTLET || pi->realWeapon == WP_BFG ) {
			angles[PITCH] = angles[ROLL];
			angles[ROLL] = 0;
		}
		AnglesToAxis( angles, barrel.axis );

		UI_PositionRotatedEntityOnTag( &barrel, &gun, pi->weaponModel, "tag_barrel");

		trap_R_AddRefEntityToScene( &barrel );
	}

	//
	// add muzzle flash
	//
	if ( dp_realtime <= pi->muzzleFlashTime ) {
		if ( pi->flashModel ) {
			memset( &flash, 0, sizeof(flash) );
			flash.hModel = pi->flashModel;
			VectorCopy( origin, flash.lightingOrigin );
			UI_PositionEntityOnTag( &flash, &gun, pi->weaponModel, "tag_flash");
			flash.renderfx = renderfx;
			trap_R_AddRefEntityToScene( &flash );
		}

		// make a dlight for the flash
		if ( pi->flashDlightColor[0] || pi->flashDlightColor[1] || pi->flashDlightColor[2] ) {
			trap_R_AddLightToScene( flash.origin, 200 + (rand()&31), pi->flashDlightColor[0],
				pi->flashDlightColor[1], pi->flashDlightColor[2] );
		}
	}
*/
	//
	// add the chat icon
	//
	if ( pi->chat ) {
		UI_PlayerFloatSprite( pi, origin, trap_R_RegisterShaderNoMip( "chatBubble" ) );
	}

	//
	// add an accent light
	//
	// not on a transformation: its lightingDynamic cel stages would saturate to white
	origin[0] -= 0;//100;	// + = behind, - = in front
	origin[1] += 100;	// + = left, - = right
	origin[2] += 0;//100;	// + = above, - = below
	if ( !pi->aura.show ) {
		trap_R_AddLightToScene( origin, 500, 1.0, 1.0, 1.0 );
	}
/*
	origin[0] -= 100;
	origin[1] -= 100;
	origin[2] -= 100;
	trap_R_AddLightToScene( origin, 500, 1.0, 0.0, 0.0 );
*/
	trap_R_RenderScene( &refdef );
}
void UI_DrawPlayer_zMesh( float x, float y, float w, float h, playerInfo_t *pi, int time ) {
	refdef_t		refdef;
	refEntity_t		legs;
	refEntity_t		torso;
	refEntity_t		head;
	refEntity_t		gun;
	refEntity_t		barrel;
	refEntity_t		flash;
	vec3_t			origin;
	int				renderfx;
	vec3_t			mins = {-16, -16, -24};
	vec3_t			maxs = {16, 16, 32};
	float			len;
	float			xx;

	if ( !pi->legsModel || !pi->animations[0].numFrames ) {
		return;
	}

	dp_realtime = time;

	if ( pi->pendingWeapon != -1 && dp_realtime > pi->weaponTimer ) {
		pi->weapon = pi->pendingWeapon;
		pi->lastWeapon = pi->pendingWeapon;
		pi->pendingWeapon = -1;
		pi->weaponTimer = 0;
	}

	UI_AdjustFrom640( &x, &y, &w, &h );

	y -= jumpHeight;

	memset( &refdef, 0, sizeof( refdef ) );
	memset( &legs, 0, sizeof(legs) );

	refdef.rdflags = RDF_NOWORLDMODEL;

	AxisClear( refdef.viewaxis );

	refdef.x = x;
	refdef.y = y;
	refdef.width = w;
	refdef.height = h;

	refdef.fov_x = (int)((float)refdef.width / 640.0f * 35.0f);
	xx = refdef.width / tan( refdef.fov_x / 360 * M_PI );
	refdef.fov_y = atan2( refdef.height, xx );
	refdef.fov_y *= ( 360 / M_PI );

	// calculate distance so the player nearly fills the box
	len = 0.6 * ( maxs[2] - mins[2] );
	origin[0] = len / tan( DEG2RAD(refdef.fov_x) * 0.5 );
	origin[1] = 0.5 * ( mins[1] + maxs[1] );
	origin[2] = -0.5 * ( mins[2] + maxs[2] );

	refdef.time = dp_realtime;

	trap_R_ClearScene();

	// get the rotation information
	UI_PlayerAngles( pi, legs.axis, torso.axis, head.axis );

	// get the animation state (after rotation, to allow feet shuffle)
	UI_PlayerAnimation( pi, &legs.oldframe, &legs.frame, &legs.backlerp,
		 &torso.oldframe, &torso.frame, &torso.backlerp,
		 &head.oldframe, &head.frame, &head.backlerp);

	renderfx = RF_LIGHTING_ORIGIN | RF_NOSHADOW;

	//
	// add the legs
	//
	legs.hModel = pi->legsModel;
	legs.customSkin = pi->legsSkin;

	VectorCopy( origin, legs.origin );

	VectorCopy( origin, legs.lightingOrigin );
	legs.renderfx = renderfx;
	VectorCopy (legs.origin, legs.oldorigin);

	trap_R_AddRefEntityToScene( &legs );

	if (!legs.hModel) {
		return;
	}

	//
	// add the chat icon
	//
	if ( pi->chat ) {
		UI_PlayerFloatSprite( pi, origin, trap_R_RegisterShaderNoMip( "chatBubble" ) );
	}

	//
	// add an accent light
	//
	origin[0] -= 0;//100;	// + = behind, - = in front
	origin[1] += 100;	// + = left, - = right
	origin[2] += 0;//100;	// + = above, - = below
	trap_R_AddLightToScene( origin, 500, 1.0, 1.0, 1.0 );
/*
	origin[0] -= 100;
	origin[1] -= 100;
	origin[2] -= 100;
	trap_R_AddLightToScene( origin, 500, 1.0, 0.0, 0.0 );
*/
	trap_R_RenderScene( &refdef );
}


/*
==========================
UI_RegisterClientSkin
==========================
*/
static void UI_RegisterTierSkin( qhandle_t *skin, const char *modelName, int tier, const char *prefix, const char *skinName ) {
	char	filename[MAX_QPATH];

	Com_sprintf( filename, sizeof( filename ), "players//%s/tier%i/%s%s.skin", modelName, tier, prefix, skinName );
	if ( trap_FS_FOpenFile( filename, 0, FS_READ ) > 0 ) {
		*skin = trap_R_RegisterSkin( filename );
	}
}

static qboolean UI_RegisterClientSkin( playerInfo_t *pi, const char *modelName, const char *skinName ) {
	char		filename[MAX_QPATH];
	int			tier;

	// like cg_tiers.c: a transformation keeps the skins of the tier below where it has none
	pi->legsSkin = pi->torsoSkin = pi->headSkin = 0;
	for ( tier = 1; tier <= ui_loadTier + 1; tier++ ) {
		Com_sprintf( filename, sizeof( filename ), "players//%s/tier%i/%s.skin", modelName, tier, skinName );
		if ( trap_FS_FOpenFile( filename, 0, FS_READ ) > 0 ) {
			pi->legsSkin = pi->torsoSkin = pi->headSkin = trap_R_RegisterSkin( filename );
			continue;
		}
		UI_RegisterTierSkin( &pi->legsSkin, modelName, tier, "lower_", skinName );
		UI_RegisterTierSkin( &pi->torsoSkin, modelName, tier, "upper_", skinName );
		UI_RegisterTierSkin( &pi->headSkin, modelName, tier, "head_", skinName );
	}

	if ( !pi->legsSkin || !pi->torsoSkin || !pi->headSkin ) {
		return qfalse;
	}

	return qtrue;
}


/*
======================
UI_ParseAnimationFile
======================
*/
static qboolean UI_ParseAnimationFile( const char *filename, animation_t *animations, playerInfo_t *pi ) {
	char		*text_p, *prev;
	int			len;
	int			i;
	char		*token;
	float		fps;
	int			skip;
	char		text[20000];
	fileHandle_t	f;

	memset( animations, 0, sizeof( animation_t ) * MAX_ANIMATIONS );

	// load the file
	len = trap_FS_FOpenFile( filename, &f, FS_READ );
	if ( len <= 0 ) {
		return qfalse;
	}
	if ( len >= ( sizeof( text ) - 1 ) ) {
		Com_Printf( "File %s too long\n", filename );
		trap_FS_FCloseFile( f );
		return qfalse;
	}
	trap_FS_Read( text, len, f );
	text[len] = 0;
	trap_FS_FCloseFile( f );

	// parse the text
	text_p = text;
	skip = 0;	// quite the compiler warning

	pi->fixedlegs = qfalse;
	pi->fixedtorso = qfalse;
	pi->overrideHead = qfalse;

	// read optional parameters
	while ( 1 ) {
		prev = text_p;	// so we can unget
		token = COM_Parse( &text_p );
		if ( !token ) {
			break;
		}
		if ( !Q_stricmp( token, "footsteps" ) ) {
			token = COM_Parse( &text_p );
			if ( !token ) {
				break;
			}
			continue;
		} else if ( !Q_stricmp( token, "headoffset" ) ) {
			for ( i = 0 ; i < 3 ; i++ ) {
				token = COM_Parse( &text_p );
				if ( !token ) {
					break;
				}
			}
			continue;
		} else if ( !Q_stricmp( token, "sex" ) ) {
			token = COM_Parse( &text_p );
			if ( !token ) {
				break;
			}
			continue;
		} else if ( !Q_stricmp( token, "fixedlegs" ) ) {
			pi->fixedlegs = qtrue;
			continue;
		} else if ( !Q_stricmp( token, "fixedtorso" ) ) {
			pi->fixedtorso = qtrue;
			continue;
		} else if ( !Q_stricmp( token, "overrideHead" ) ) {
			pi->overrideHead = qtrue;
			continue;
		}

		// if it is a number, start parsing animations
		if ( token[0] >= '0' && token[0] <= '9' ) {
			text_p = prev;	// unget the token
			break;
		}

		Com_Printf( "unknown token '%s' is %s\n", token, filename );
	}

	// read information for each frame
	for ( i = 0 ; i < MAX_ANIMATIONS ; i++ ) {

		token = COM_Parse( &text_p );
		if ( !token ) {
			break;
		}
		animations[i].firstFrame = atoi( token );
/*
		// leg only frames are adjusted to not count the upper body only frames
		if ( i == ANIM_WALKCR ) {
			skip = animations[ANIM_WALKCR].firstFrame - animations[ANIM_GESTURE].firstFrame;
		}
		if ( i >= ANIM_WALKCR ) {
			animations[i].firstFrame -= skip;
		}
*/
		token = COM_Parse( &text_p );
		if ( !token ) {
			break;
		}
		animations[i].numFrames = atoi( token );

		token = COM_Parse( &text_p );
		if ( !token ) {
			break;
		}
		animations[i].loopFrames = atoi( token );

		token = COM_Parse( &text_p );
		if ( !token ) {
			break;
		}
		fps = atof( token );
		if ( fps == 0 ) {
			fps = 1;
		}
		animations[i].frameLerp = 1000 / fps;
		animations[i].initialLerp = 1000 / fps;

		// ADDING FOR ZEQ2

		// Read the continuous flag for ki attack animations
		if ( i >= ANIM_KI_ATTACK1_FIRE &&
			 i < MAX_ANIMATIONS &&
			 (i - ANIM_KI_ATTACK1_FIRE) % 2 == 0 ) {
			token = COM_Parse( &text_p );
			if ( !*token ) {
				break;
			}
			animations[i].continuous = atoi( token );
		}

		// END ADDING
	}

	if ( i != MAX_ANIMATIONS ) {
		Com_Printf( "Error parsing animation file: %s", filename );
		return qfalse;
	}

	return qtrue;
}


/*
==========================
UI_RegisterClientModelname
==========================
*/
// the md3 of the tier being loaded, or of the closest tier below that has one
static qhandle_t UI_RegisterTierModel( const char *modelName, const char *file ) {
	char		filename[MAX_QPATH];
	qhandle_t	model;
	int			tier;

	model = 0;
	for ( tier = 1; tier <= ui_loadTier + 1; tier++ ) {
		Com_sprintf( filename, sizeof( filename ), "players//%s/tier%i/%s", modelName, tier, file );
		if ( trap_FS_FOpenFile( filename, 0, FS_READ ) > 0 ) {
			model = trap_R_RegisterModel( filename );
		}
	}
	return model;
}

qboolean UI_RegisterClientModelname( playerInfo_t *pi, const char *modelSkinName ) {
	char		modelName[MAX_QPATH];
	char		skinName[MAX_QPATH];
	char		filename[MAX_QPATH];
	char		*slash;

	pi->torsoModel = 0;
	pi->headModel = 0;

	if ( !modelSkinName[0] ) {
		return qfalse;
	}

	Q_strncpyz( modelName, modelSkinName, sizeof( modelName ) );

	slash = strchr( modelName, '/' );
	if ( !slash ) {
		// modelName did not include a skin name
		Q_strncpyz( skinName, "default", sizeof( skinName ) );
	} else {
		Q_strncpyz( skinName, slash + 1, sizeof( skinName ) );
		// truncate modelName
		*slash = 0;
	}

	// load cmodels before models so filecache works

	Com_sprintf( filename, sizeof( filename ), "players//%s/tier1/body.zMesh", modelName );
	pi->legsModel = trap_R_RegisterModel( filename );
	if ( !pi->legsModel ) {
		//Com_Printf( "Failed to load model file %s, trying players//%s/tier1/body.zMesh\n", filename, modelName );


		pi->legsModel = UI_RegisterTierModel( modelName, "lower.md3" );
		if ( !pi->legsModel ) {
			//Com_Printf( "Failed to load model file %s\n", filename );
			return qfalse;
		}

		pi->torsoModel = UI_RegisterTierModel( modelName, "upper.md3" );
		if ( !pi->torsoModel ) {
			//Com_Printf( "Failed to load model file %s\n", filename );
			return qfalse;
		}

		pi->headModel = UI_RegisterTierModel( modelName, "head.md3" );
		if ( !pi->headModel ) {
			//Com_Printf( "Failed to load model file %s\n", filename );
			return qfalse;
		}
	}
	// if any skins failed to load, fall back to default
	if ( !UI_RegisterClientSkin( pi, modelName, skinName ) ) {
		if ( !UI_RegisterClientSkin( pi, modelName, "default" ) ) {
			//Com_Printf( "Failed to load skin file: %s : %s\n", modelName, skinName );
			return qfalse;
		}
	}

	// load the animations
	Com_sprintf( filename, sizeof( filename ), "players//%s/animation.cfg", modelName );
	if ( !UI_ParseAnimationFile( filename, pi->animations, pi ) ) {
		//Com_Printf( "Failed to load animation file %s\n", filename );
		return qfalse;
	}

	return qtrue;
}


/*
===============
UI_PlayerInfo_SetModel
===============
*/
void UI_PlayerInfo_SetModel( playerInfo_t *pi, const char *model ) {
	memset( pi, 0, sizeof(*pi) );
	UI_RegisterClientModelname( pi, model );
	pi->weapon = WP_MACHINEGUN;
	pi->currentWeapon = pi->weapon;
	pi->lastWeapon = pi->weapon;
	pi->pendingWeapon = -1;
	pi->weaponTimer = 0;
	pi->chat = qfalse;
	pi->newModel = qtrue;
	UI_PlayerInfo_SetWeapon( pi, pi->weapon );
}


/*
===============
UI_PlayerInfo_SetModelTier

a transformation of the fighter (tier 0 = base form) with its aura and
lightning, for the Fighter preview
===============
*/
void UI_PlayerInfo_SetModelTier( playerInfo_t *pi, const char *model, int tier ) {
	char	modelName[MAX_QPATH];
	char	*slash;

	ui_loadTier = tier;
	UI_PlayerInfo_SetModel( pi, model );
	ui_loadTier = 0;

	Q_strncpyz( modelName, model, sizeof( modelName ) );
	slash = strchr( modelName, '/' );
	if ( slash ) {
		*slash = 0;
	}
	UI_LoadAura( &pi->aura, modelName, tier );
}


/*
===============
UI_PlayerInfo_SetInfo
===============
*/
void UI_PlayerInfo_SetInfo( playerInfo_t *pi, int legsAnim, int torsoAnim, vec3_t viewAngles, vec3_t moveAngles, weapon_t weaponNumber, qboolean chat ) {
	int			currentAnim;
	weapon_t	weaponNum;

	pi->chat = chat;

	// view angles
	VectorCopy( viewAngles, pi->viewAngles );

	// move angles
	VectorCopy( moveAngles, pi->moveAngles );

	if ( pi->newModel ) {
		pi->newModel = qfalse;

		jumpHeight = 0;
		pi->pendingLegsAnim = 0;
		UI_ForceLegsAnim( pi, legsAnim );
		pi->legs.yawAngle = viewAngles[YAW];
		pi->legs.yawing = qfalse;

		pi->pendingTorsoAnim = 0;
		UI_ForceTorsoAnim( pi, torsoAnim );
		pi->torso.yawAngle = viewAngles[YAW];
		pi->torso.yawing = qfalse;
/*
		pi->pendingHeadAnim = 0;
		UI_ForceHeadAnim( pi, headAnim );
		pi->head.yawAngle = viewAngles[YAW];
		pi->head.yawing = qfalse;
*/
		if ( weaponNumber != -1 ) {
			pi->weapon = weaponNumber;
			pi->currentWeapon = weaponNumber;
			pi->lastWeapon = weaponNumber;
			pi->pendingWeapon = -1;
			pi->weaponTimer = 0;
			UI_PlayerInfo_SetWeapon( pi, pi->weapon );
		}

		return;
	}

	// weapon
	if ( weaponNumber == -1 ) {
		pi->pendingWeapon = -1;
		pi->weaponTimer = 0;
	}
	else if ( weaponNumber != WP_NONE ) {
		pi->pendingWeapon = weaponNumber;
		pi->weaponTimer = dp_realtime + UI_TIMER_WEAPON_DELAY;
	}
	weaponNum = pi->lastWeapon;
	pi->weapon = weaponNum;

	if ( torsoAnim == ANIM_DEATH_GROUND || legsAnim == ANIM_DEATH_GROUND ) {
		torsoAnim = legsAnim = ANIM_DEATH_GROUND;
		pi->weapon = pi->currentWeapon = WP_NONE;
		UI_PlayerInfo_SetWeapon( pi, pi->weapon );

		jumpHeight = 0;
		pi->pendingLegsAnim = 0;
		UI_ForceLegsAnim( pi, legsAnim );

		pi->pendingTorsoAnim = 0;
		UI_ForceTorsoAnim( pi, torsoAnim );

		return;
	}

	// leg animation
	currentAnim = pi->legsAnim & ~ANIM_TOGGLEBIT;
	if ( legsAnim != ANIM_JUMP_UP && ( currentAnim == ANIM_JUMP_UP || currentAnim == ANIM_LAND_UP ) ) {
		pi->pendingLegsAnim = legsAnim;
	}
	else if ( legsAnim != currentAnim ) {
		jumpHeight = 0;
		pi->pendingLegsAnim = 0;
		UI_ForceLegsAnim( pi, legsAnim );
	}

	// torso animation
	if ( torsoAnim == ANIM_IDLE || torsoAnim == ANIM_IDLE_LOCKED ) {
		if ( weaponNum == WP_NONE || weaponNum == WP_GAUNTLET ) {
			torsoAnim = ANIM_IDLE;
		}
		else {
			torsoAnim = ANIM_IDLE;
		}
	}
/*
	if ( torsoAnim == ANIM_ATTACK || torsoAnim == ANIM_ATTACK2 ) {
		if ( weaponNum == WP_NONE || weaponNum == WP_GAUNTLET ) {
			torsoAnim = ANIM_ATTACK2;
		}
		else {
			torsoAnim = ANIM_ATTACK;
		}
		pi->muzzleFlashTime = dp_realtime + UI_TIMER_MUZZLE_FLASH;
		//FIXME play firing sound here
	}
*/
	currentAnim = pi->torsoAnim & ~ANIM_TOGGLEBIT;
/*
	if ( weaponNum != pi->currentWeapon || currentAnim == ANIM_RAISE || currentAnim == ANIM_DROP ) {
		pi->pendingTorsoAnim = torsoAnim;
	}
	else if ( ( currentAnim == ANIM_GESTURE || currentAnim == ANIM_ATTACK ) && ( torsoAnim != currentAnim ) ) {
		pi->pendingTorsoAnim = torsoAnim;
	}
	else */if ( torsoAnim != currentAnim ) {
		pi->pendingTorsoAnim = 0;
		UI_ForceTorsoAnim( pi, torsoAnim );
	}

}
