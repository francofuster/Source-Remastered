#include "g_local.h"

static qboolean BotLite_TargetStillRecovering( gentity_t *target );
static qboolean BotLite_RunPostCrashFlyup( gentity_t *bot, int clientNum );

static qboolean BotLite_TargetNeedsRecoveryWait( gentity_t *target );
static void BotLite_StartRecoveryWait( gentity_t *bot, int clientNum, gentity_t *target );
static qboolean BotLite_RunRecoveryWait( gentity_t *bot, int clientNum, gentity_t *target );


/* BotLite forward declarations */
static void BotLite_ApplyViewAngles( gentity_t *bot, const vec3_t angles );
static void BotLite_SetLockOn( gentity_t *bot, gentity_t *target );
static void BotLite_FaceTarget( gentity_t *bot, gentity_t *target );

#define BOTLITE_MAX_BOTS MAX_CLIENTS
#define BOTLITE_ACQUIRE_DISTANCE 10000.0f
#define BOTLITE_COMBAT_DISTANCE 64.0f
#define BOTLITE_MELEE_START_DISTANCE 10000.0f
#define BOTLITE_MELEE_CHASE_DISTANCE 10000.0f
#define BOTLITE_LOCK_RANGE 2200.0f
#define BOTLITE_SEARCH_RISE_TIME 3000
#define BOTLITE_SEARCH_TURN_TIME 450
#define BOTLITE_SEARCH_FORWARD_TIME 3000
#define BOTLITE_SEARCH_TURN_MIN 35.0f
#define BOTLITE_SEARCH_TURN_MAX 120.0f
#define BOTLITE_VIEW_HEIGHT 48.0f
#define BOTLITE_SKILL2_RANGED_DISTANCE 10000.0f
#define BOTLITE_SKILL2_MELEE_EXIT_DISTANCE 10000.0f
#define BOTLITE_SKILL2_APPROACH_TRIGGER_DISTANCE 1500.0f
#define BOTLITE_SKILL2_APPROACH_INTERVAL 5000
#define BOTLITE_SKILL2_APPROACH_TIME 1200
#define BOTLITE_SKILL2_WEAPON_SWITCH_TIME 5000
#define BOTLITE_SKILL2_CHARGE_MIN 25

typedef enum {
	BOTLITE_MODE_IDLE = 0,
	BOTLITE_MODE_SEARCH,
	BOTLITE_MODE_COMBAT,
	BOTLITE_MODE_WAIT_TARGET_RECOVERY
} botlite_mode_t;

typedef enum {
	BOTLITE_COMBO_PUNCH = 0,
	BOTLITE_COMBO_KICK,
	BOTLITE_COMBO_SPEED,
	BOTLITE_COMBO_FINISH
} botlite_combo_stage_t;

typedef struct {
	qboolean inuse;
	int skill;
	char character[MAX_QPATH];
	botlite_mode_t mode;
	qboolean didInitialRise;
	int riseEndTime;
	int turnEndTime;
	int moveEndTime;
	float turnYawStart;
	float moveYaw;
	int lastTargetNum;
	int comboStage;
	int comboStep;
	int actionUntil;
	int nextActionTime;
	int lastDamageEvent;
	qboolean debugEnabled;
	int crashStartTime;
	int lastLoggedMode;
	int skill2Weapon;
	int skill2AttackMode;
	int skill2WeaponSwitchTime;
	int skill2HoldUntil;
	int skill2ApproachTime;
	qboolean skill2ForceMelee;
	int retreatUntil;
	float retreatYaw;
	qboolean postCrashFlyup;
	int postCrashRiseEndTime;
} botlite_info_t;

static botlite_info_t g_botlite[BOTLITE_MAX_BOTS];

static void BotLite_DebugLog( gentity_t *bot, const char *msg ) {
	botlite_info_t *info;

	if ( !bot || !bot->client ) {
		return;
	}

	info = &g_botlite[bot->s.number];
	if ( !info->debugEnabled ) {
		return;
	}

	G_Printf( "[BOTDEBUG:%s:%d] %s\n", bot->client->pers.netname, bot->s.number, msg );
}

static void BotLite_FailsafeRecoverCrash( gentity_t *bot, botlite_info_t *info ) {
	if ( !bot || !bot->client || !info ) {
		return;
	}

	if ( !( bot->client->ps.bitFlags & isCrashed ) &&
		 !( bot->client->ps.bitFlags & isUnconcious ) &&
		 bot->client->ps.timers[tmCrash] == 0 &&
		 bot->client->ps.timers[tmRecover] == 0 &&
		 bot->client->ps.timers[tmKnockback] == 0 ) {
		info->crashStartTime = 0;
		return;
	}

	if ( info->crashStartTime <= 0 ) {
		info->crashStartTime = level.time;
		BotLite_DebugLog( bot, "Crash state started" );
		return;
	}

	if ( level.time - info->crashStartTime < 3000 ) {
		return;
	}

	bot->client->ps.bitFlags &= ~isCrashed;
	bot->client->ps.bitFlags &= ~isUnconcious;
	bot->client->ps.powerups[PW_STATE] = 0;
	bot->client->ps.timers[tmCrash] = 0;
	bot->client->ps.timers[tmRecover] = 0;
	bot->client->ps.timers[tmKnockback] = 0;
	info->nextActionTime = 0;
	info->actionUntil = 0;
	info->comboStep = 0;
	info->crashStartTime = 0;

	BotLite_DebugLog( bot, "Failsafe recovery applied" );
}

static qboolean BotLite_IsTemporarilyDisabled( gentity_t *bot ) {
	botlite_info_t *info;

	if ( !bot || !bot->client ) {
		return qtrue;
	}

	info = &g_botlite[bot->s.number];
	BotLite_FailsafeRecoverCrash( bot, info );

	if ( ( bot->client->ps.bitFlags & isCrashed ) ||
		 ( bot->client->ps.bitFlags & isUnconcious ) ||
		 bot->client->ps.timers[tmCrash] > 0 ||
		 bot->client->ps.timers[tmKnockback] > 0 ||
		 bot->client->ps.timers[tmRecover] > 0 ) {
		return qtrue;
	}

	return qfalse;
}


static qboolean BotLite_IsManagedBot( int clientNum ) {
	if ( clientNum < 0 || clientNum >= level.maxclients ) {
		return qfalse;
	}
	return g_botlite[clientNum].inuse;
}

static int BotLite_ClampSkill( int skill ) {
	if ( skill < 1 ) {
		return 1;
	}
	if ( skill > 3 ) {
		return 3;
	}
	return skill;
}

void BotLite_ResetAll( void ) {
	memset( g_botlite, 0, sizeof( g_botlite ) );
}

void BotLite_OnClientDisconnect( int clientNum ) {
	if ( clientNum < 0 || clientNum >= level.maxclients ) {
		return;
	}
	memset( &g_botlite[clientNum], 0, sizeof( g_botlite[clientNum] ) );
	g_botlite[clientNum].lastLoggedMode = -1;
}

static float BotLite_RandomRange( float minv, float maxv ) {
	return minv + random() * ( maxv - minv );
}


static float BotLite_AngleNormalize360( float angle ) {
	while ( angle < 0.0f ) {
		angle += 360.0f;
	}
	while ( angle >= 360.0f ) {
		angle -= 360.0f;
	}
	return angle;
}

static float BotLite_ShortestAngleDelta( float from, float to ) {
	float delta;
	from = BotLite_AngleNormalize360( from );
	to = BotLite_AngleNormalize360( to );
	delta = to - from;
	while ( delta > 180.0f ) {
		delta -= 360.0f;
	}
	while ( delta < -180.0f ) {
		delta += 360.0f;
	}
	return delta;
}

static void BotLite_ClearLock( gentity_t *bot ) {
	if ( !bot || !bot->client ) {
		return;
	}
	if ( bot->client->ps.lockedTarget > 0 && bot->client->ps.lockedPlayer ) {
		bot->client->ps.lockedPlayer->bitFlags &= ~isTargeted;
	}
	bot->client->ps.lockedTarget = 0;
	bot->client->ps.lockedPlayer = NULL;
	bot->client->ps.lockedPosition = NULL;
}

static qboolean BotLite_TargetIsValid( gentity_t *bot, gentity_t *target ) {
	if ( !bot || !target || !target->inuse || !target->client ) {
		return qfalse;
	}
	if ( target == bot ) {
		return qfalse;
	}
	if ( target->client->pers.connected != CON_CONNECTED ) {
		return qfalse;
	}
	if ( target->client->sess.sessionTeam == TEAM_SPECTATOR ) {
		return qfalse;
	}
	if ( target->r.svFlags & SVF_BOT ) {
		return qfalse;
	}
	if ( target->client->ps.bitFlags & isDead ) {
		return qfalse;
	}
	if ( bot->client->sess.sessionTeam >= TEAM_RED && target->client->sess.sessionTeam == bot->client->sess.sessionTeam ) {
		return qfalse;
	}
	return qtrue;
}

static gentity_t *BotLite_FindNearestVisiblePlayer( gentity_t *bot, float *outDistSq );
static gentity_t *BotLite_FindNearestPlayerAnyDistance( gentity_t *bot, float *outDistSq );
static gentity_t *BotLite_GetLastAttacker( gentity_t *bot );
static int BotLite_PickSkill2Weapon( gentity_t *bot, int currentWeapon );
static void BotLite_StartSkill2Attack( gentity_t *bot, int clientNum );
static void BotLite_RunCombatSkill2( gentity_t *bot, int clientNum, gentity_t *target, float distSq );

static gentity_t *BotLite_GetLastAttacker( gentity_t *bot ) {
	int attackerNum;
	gentity_t *attacker;

	if ( !bot || !bot->client ) {
		return NULL;
	}

	attackerNum = bot->client->lasthurt_client;
	if ( attackerNum < 0 || attackerNum >= level.maxclients ) {
		attackerNum = bot->client->ps.persistant[PERS_ATTACKER];
	}

	if ( attackerNum < 0 || attackerNum >= level.maxclients ) {
		return NULL;
	}

	attacker = &g_entities[attackerNum];
	if ( !BotLite_TargetIsValid( bot, attacker ) ) {
		return NULL;
	}

	return attacker;
}

static gentity_t *BotLite_FindBestDamageAttacker( gentity_t *bot, float *outDistSq ) {
	int i;
	gentity_t *best;
	float bestScore;
	float damageYaw;

	best = NULL;
	bestScore = 9999999.0f;
	damageYaw = ( bot->client->ps.damageYaw / 256.0f ) * 360.0f;

	for ( i = 0; i < level.maxclients; i++ ) {
		gentity_t *other;
		vec3_t delta;
		vec3_t angles;
		float distSq;
		float yawDiff;
		float score;

		other = &g_entities[i];
		if ( !BotLite_TargetIsValid( bot, other ) ) {
			continue;
		}

		VectorSubtract( other->client->ps.origin, bot->client->ps.origin, delta );
		distSq = VectorLengthSquared( delta );
		if ( distSq > ( 4096.0f * 4096.0f ) ) {
			continue;
		}

		vectoangles( delta, angles );
		yawDiff = fabs( BotLite_ShortestAngleDelta( damageYaw, angles[YAW] ) );
		score = yawDiff * 12.0f + distSq / 2048.0f;

		if ( score < bestScore ) {
			bestScore = score;
			best = other;
			if ( outDistSq ) {
				*outDistSq = distSq;
			}
		}
	}

	if ( !best ) {
		return BotLite_FindNearestPlayerAnyDistance( bot, outDistSq );
	}

	return best;
}

static void BotLite_FaceDamageDirection( gentity_t *bot ) {
	vec3_t angles;

	VectorCopy( bot->client->ps.viewangles, angles );
	angles[YAW] = ( bot->client->ps.damageYaw / 256.0f ) * 360.0f;
	angles[PITCH] = 0;
	angles[ROLL] = 0;
	BotLite_ApplyViewAngles( bot, angles );
}

static qboolean BotLite_ReactToDamage( gentity_t *bot, int clientNum, float *outDistSq ) {
	botlite_info_t *info;
	gentity_t *attacker;
	vec3_t delta;
	float distSq;

	info = &g_botlite[clientNum];
	if ( bot->client->ps.damageEvent == info->lastDamageEvent || bot->client->ps.damageCount <= 0 ) {
		return qfalse;
	}

	info->lastDamageEvent = bot->client->ps.damageEvent;
	BotLite_DebugLog( bot, "Received damage" );
	info->mode = BOTLITE_MODE_COMBAT;
	info->nextActionTime = 0;
	info->actionUntil = 0;

	attacker = BotLite_GetLastAttacker( bot );
	if ( !attacker ) {
		attacker = BotLite_FindBestDamageAttacker( bot, &distSq );
	}
	if ( !attacker ) {
		attacker = BotLite_FindNearestPlayerAnyDistance( bot, &distSq );
	}

	if ( attacker ) {
		VectorSubtract( attacker->client->ps.origin, bot->client->ps.origin, delta );
		distSq = VectorLengthSquared( delta );
		info->lastTargetNum = attacker->s.number;
		BotLite_DebugLog( bot, "Damage reaction target acquired" );
		BotLite_SetLockOn( bot, attacker );
		BotLite_FaceTarget( bot, attacker );
		if ( outDistSq ) {
			*outDistSq = distSq;
		}
		return qtrue;
	}

	info->lastTargetNum = -1;
	BotLite_FaceDamageDirection( bot );
	if ( outDistSq ) {
		*outDistSq = BOTLITE_LOCK_RANGE * BOTLITE_LOCK_RANGE;
	}
	return qtrue;
}

static gentity_t *BotLite_GetTrackedTarget( gentity_t *bot, int clientNum, float *outDistSq ) {
	botlite_info_t *info;
	gentity_t *target;
	vec3_t delta;
	float distSq;

	info = &g_botlite[clientNum];
	target = NULL;
	distSq = BOTLITE_LOCK_RANGE * BOTLITE_LOCK_RANGE;

	if ( info->lastTargetNum >= 0 && info->lastTargetNum < level.maxclients ) {
		target = &g_entities[info->lastTargetNum];
		if ( !BotLite_TargetIsValid( bot, target ) ) {
			target = NULL;
		} else {
			VectorSubtract( target->client->ps.origin, bot->client->ps.origin, delta );
			distSq = VectorLengthSquared( delta );
		}
	}

	if ( !target ) {
		target = BotLite_FindNearestVisiblePlayer( bot, &distSq );
		if ( target ) {
			info->lastTargetNum = target->s.number;
		}
	}

	if ( outDistSq ) {
		*outDistSq = distSq;
	}
	return target;
}

static gentity_t *BotLite_FindNearestPlayerAnyDistance( gentity_t *bot, float *outDistSq ) {
	int i;
	gentity_t *best;
	float bestDistSq;

	best = NULL;
	bestDistSq = 999999999.0f;

	for ( i = 0; i < level.maxclients; i++ ) {
		gentity_t *other;
		vec3_t delta;
		float distSq;

		other = &g_entities[i];
		if ( !BotLite_TargetIsValid( bot, other ) ) {
			continue;
		}

		VectorSubtract( other->client->ps.origin, bot->client->ps.origin, delta );
		distSq = VectorLengthSquared( delta );
		if ( distSq >= bestDistSq ) {
			continue;
		}

		best = other;
		bestDistSq = distSq;
	}

	if ( outDistSq ) {
		*outDistSq = best ? bestDistSq : ( BOTLITE_LOCK_RANGE * BOTLITE_LOCK_RANGE );
	}
	return best;
}

static gentity_t *BotLite_FindNearestVisiblePlayer( gentity_t *bot, float *outDistSq ) {
	int i;
	gentity_t *best;
	float bestDistSq;

	best = NULL;
	bestDistSq = BOTLITE_ACQUIRE_DISTANCE * BOTLITE_ACQUIRE_DISTANCE;

	for ( i = 0; i < level.maxclients; i++ ) {
		gentity_t *other;
		vec3_t delta;
		float distSq;

		other = &g_entities[i];
		if ( !BotLite_TargetIsValid( bot, other ) ) {
			continue;
		}

		VectorSubtract( other->client->ps.origin, bot->client->ps.origin, delta );
		distSq = VectorLengthSquared( delta );
		if ( distSq >= bestDistSq ) {
			continue;
		}

		best = other;
		bestDistSq = distSq;
	}

	if ( outDistSq ) {
		*outDistSq = bestDistSq;
	}
	return best;
}

static void BotLite_ApplyViewAngles( gentity_t *bot, const vec3_t angles ) {
	vec3_t fixedAngles;

	VectorCopy( angles, fixedAngles );
	fixedAngles[ROLL] = 0;
	SetClientViewAngle( bot, fixedAngles );
	VectorCopy( fixedAngles, bot->r.currentAngles );
}

static void BotLite_FacePosition( gentity_t *bot, const vec3_t targetPos ) {
	vec3_t dir;
	vec3_t angles;

	VectorSubtract( targetPos, bot->client->ps.origin, dir );
	vectoangles( dir, angles );
	angles[ROLL] = 0;
	BotLite_ApplyViewAngles( bot, angles );
}

static void BotLite_FaceTarget( gentity_t *bot, gentity_t *target ) {
	vec3_t aimPos;
	BotLite_DebugLog( bot, "Face target" );
	VectorCopy( target->client->ps.origin, aimPos );
	aimPos[2] += BOTLITE_VIEW_HEIGHT;
	BotLite_FacePosition( bot, aimPos );
}

static void BotLite_SetLockOn( gentity_t *bot, gentity_t *target ) {
	BotLite_DebugLog( bot, "Set lock-on" );
	if ( !bot || !bot->client || !BotLite_TargetIsValid( bot, target ) ) {
		return;
	}
	bot->client->ps.lockedTarget = target->s.number + 1;
	bot->client->ps.lockedPlayer = &target->client->ps;
	bot->client->ps.lockedPosition = &target->r.currentOrigin;
	target->client->ps.bitFlags |= isTargeted;
}

static void BotLite_SetMoveYaw( gentity_t *bot, int clientNum ) {
	botlite_info_t *info;
	float currentYaw;
	float yawDelta;

	info = &g_botlite[clientNum];
	currentYaw = bot->client->ps.viewangles[YAW];
	yawDelta = BotLite_RandomRange( BOTLITE_SEARCH_TURN_MIN, BOTLITE_SEARCH_TURN_MAX );
	if ( rand() & 1 ) {
		yawDelta = -yawDelta;
	}
	info->turnYawStart = BotLite_AngleNormalize360( currentYaw );
	info->moveYaw = BotLite_AngleNormalize360( currentYaw + yawDelta );
	info->turnEndTime = level.time + BOTLITE_SEARCH_TURN_TIME;
	info->moveEndTime = info->turnEndTime + BOTLITE_SEARCH_FORWARD_TIME;
}




static qboolean BotLite_TargetStillRecovering( gentity_t *target ) {
	if ( !target || !target->client ) {
		return qfalse;
	}

	if ( ( target->client->ps.bitFlags & isCrashed ) ||
		 ( target->client->ps.bitFlags & isUnconcious ) ||
		 target->client->ps.timers[tmCrash] > 0 ||
		 target->client->ps.timers[tmRecover] > 0 ||
		 target->client->ps.powerups[PW_STATE] == -1 ) {
		return qtrue;
	}

	return qfalse;
}

static qboolean BotLite_RunPostCrashFlyup( gentity_t *bot, int clientNum ) {
	botlite_info_t *info;
	usercmd_t *cmd;

	info = &g_botlite[clientNum];
	if ( !info->postCrashFlyup ) {
		return qfalse;
	}

	cmd = &bot->client->pers.cmd;
	if ( info->postCrashRiseEndTime <= 0 ) {
		info->postCrashRiseEndTime = level.time + 1000;
	}

	if ( level.time < info->postCrashRiseEndTime ) {
		cmd->upmove = 127;
		return qtrue;
	}

	info->postCrashFlyup = qfalse;
	info->postCrashRiseEndTime = 0;
	info->didInitialRise = qtrue;
	info->mode = BOTLITE_MODE_SEARCH;
	info->nextActionTime = 0;
	info->actionUntil = 0;
	return qfalse;
}

static qboolean BotLite_TargetNeedsRecoveryWait( gentity_t *target ) {
	if ( !target || !target->client ) {
		return qfalse;
	}

	if ( ( target->client->ps.bitFlags & isCrashed ) ||
		 ( target->client->ps.bitFlags & isUnconcious ) ||
		 target->client->ps.timers[tmCrash] > 0 ||
		 target->client->ps.timers[tmRecover] > 0 ||
		 target->client->ps.powerups[PW_STATE] == -1 ) {
		return qtrue;
	}

	return qfalse;
}

static void BotLite_StartRecoveryWait( gentity_t *bot, int clientNum, gentity_t *target ) {
	botlite_info_t *info;
	vec3_t delta;

	info = &g_botlite[clientNum];
	info->mode = BOTLITE_MODE_WAIT_TARGET_RECOVERY;
	info->lastTargetNum = target ? target->s.number : -1;
	info->nextActionTime = 0;
	info->actionUntil = 0;
	info->comboStep = 0;
	info->retreatUntil = level.time + 2000;

	if ( target && target->client ) {
		VectorSubtract( bot->client->ps.origin, target->client->ps.origin, delta );
		if ( delta[0] == 0.0f && delta[1] == 0.0f ) {
			info->retreatYaw = bot->client->ps.viewangles[YAW];
		} else {
			info->retreatYaw = vectoyaw( delta );
		}
	} else {
		info->retreatYaw = bot->client->ps.viewangles[YAW];
	}
}

static qboolean BotLite_RunRecoveryWait( gentity_t *bot, int clientNum, gentity_t *target ) {
	botlite_info_t *info;
	usercmd_t *cmd;
	vec3_t angles;

	info = &g_botlite[clientNum];
	if ( info->mode != BOTLITE_MODE_WAIT_TARGET_RECOVERY ) {
		return qfalse;
	}

	cmd = &bot->client->pers.cmd;

	if ( target && BotLite_TargetStillRecovering( target ) ) {
		VectorClear( angles );
		angles[YAW] = info->retreatYaw;
		BotLite_ApplyViewAngles( bot, angles );

		if ( level.time < info->retreatUntil ) {
			cmd->forwardmove = 127;
			if ( info->skill == 2 ) {
				cmd->buttons |= BUTTON_BOOST;
			}
		} else {
			if ( target->client ) {
				BotLite_FaceTarget( bot, target );
			}
			cmd->forwardmove = 0;
			cmd->rightmove = 0;
			cmd->upmove = 0;
			cmd->buttons = 0;
		}
		return qtrue;
	}

	info->mode = BOTLITE_MODE_COMBAT;
	info->nextActionTime = 0;
	info->actionUntil = 0;
	info->retreatUntil = 0;
	if ( target && target->client ) {
		info->lastTargetNum = target->s.number;
		BotLite_SetLockOn( bot, target );
	}
	return qfalse;
}


static void BotLite_MoveSearchPattern( gentity_t *bot, int clientNum ) {
	botlite_info_t *info;
	usercmd_t *cmd;
	vec3_t angles;
	float yaw;

	info = &g_botlite[clientNum];
	cmd = &bot->client->pers.cmd;

	if ( !info->didInitialRise ) {
		info->didInitialRise = qtrue;
		info->riseEndTime = level.time + BOTLITE_SEARCH_RISE_TIME;
		info->turnEndTime = 0;
		info->moveEndTime = 0;
	}

	if ( level.time < info->riseEndTime ) {
		cmd->upmove = 127;
		return;
	}

	if ( level.time >= info->moveEndTime ) {
		BotLite_SetMoveYaw( bot, clientNum );
	}

	if ( level.time < info->turnEndTime ) {
		float frac;
		float delta;
		frac = (float)( level.time - ( info->turnEndTime - BOTLITE_SEARCH_TURN_TIME ) ) / (float)BOTLITE_SEARCH_TURN_TIME;
		if ( frac < 0.0f ) frac = 0.0f;
		if ( frac > 1.0f ) frac = 1.0f;
		delta = BotLite_ShortestAngleDelta( info->turnYawStart, info->moveYaw );
		yaw = BotLite_AngleNormalize360( info->turnYawStart + delta * frac );
	} else {
		yaw = info->moveYaw;
		cmd->forwardmove = 127;
	}

	VectorClear( angles );
	angles[YAW] = yaw;
	BotLite_ApplyViewAngles( bot, angles );
}

static void BotLite_RunSearchSkill1( gentity_t *bot, int clientNum, gentity_t *target ) {
	botlite_info_t *info;
	info = &g_botlite[clientNum];

	if ( target ) {
		info->mode = BOTLITE_MODE_COMBAT;
		info->lastTargetNum = target->s.number;
		info->nextActionTime = 0;
		info->actionUntil = 0;
		BotLite_DebugLog( bot, "Search acquired target" );
		BotLite_SetLockOn( bot, target );
		return;
	}

	BotLite_MoveSearchPattern( bot, clientNum );
}

static void BotLite_RunSearchDefault( gentity_t *bot, int clientNum, gentity_t *target ) {
	if ( target ) {
		g_botlite[clientNum].mode = BOTLITE_MODE_COMBAT;
		g_botlite[clientNum].lastTargetNum = target->s.number;
		BotLite_DebugLog( bot, "Default search acquired target" );
		BotLite_SetLockOn( bot, target );
	}
}

static void BotLite_StartCombo( int clientNum ) {
	botlite_info_t *info;
	info = &g_botlite[clientNum];
	info->comboStage = BOTLITE_COMBO_PUNCH + ( rand() % 2 );
	info->comboStep = 0;
	info->nextActionTime = level.time;
	info->actionUntil = 0;
	BotLite_DebugLog( &g_entities[clientNum], "Combo started" );
}

static void BotLite_AdvanceCombo( int clientNum ) {
	botlite_info_t *info;
	info = &g_botlite[clientNum];

	if ( info->comboStage == BOTLITE_COMBO_PUNCH || info->comboStage == BOTLITE_COMBO_KICK ) {
		info->comboStep++;
		if ( info->comboStep >= 2 + ( rand() % 2 ) ) {
			info->comboStep = 0;
			info->comboStage = BOTLITE_COMBO_SPEED;
		} else {
			info->comboStage = ( info->comboStage == BOTLITE_COMBO_PUNCH ) ? BOTLITE_COMBO_KICK : BOTLITE_COMBO_PUNCH;
		}
	}
	else if ( info->comboStage == BOTLITE_COMBO_SPEED ) {
		info->comboStage = BOTLITE_COMBO_FINISH;
	}
	else {
		info->comboStage = BOTLITE_COMBO_PUNCH + ( rand() % 2 );
		info->comboStep = 0;
	}

	info->nextActionTime = level.time + 110 + ( rand() % 90 );
	BotLite_DebugLog( &g_entities[clientNum], "Combo advanced" );
}

static void BotLite_ApplyComboButtons( botlite_info_t *info, usercmd_t *cmd ) {
	cmd->forwardmove = 127;

	switch ( info->comboStage ) {
	case BOTLITE_COMBO_PUNCH:
		cmd->buttons |= BUTTON_ATTACK;
		break;
	case BOTLITE_COMBO_KICK:
		cmd->buttons |= BUTTON_ALT_ATTACK;
		break;
	case BOTLITE_COMBO_SPEED:
		/* forward only: PM_Melee treats this as start / speed melee pressure */
		break;
	case BOTLITE_COMBO_FINISH:
		cmd->buttons |= BUTTON_ALT_ATTACK;
		break;
	}
}

static void BotLite_RunCombatSkill1( gentity_t *bot, int clientNum, gentity_t *target, float distSq ) {
	botlite_info_t *info;
	usercmd_t *cmd;
	float dist;
	qboolean inMelee;

	info = &g_botlite[clientNum];
	cmd = &bot->client->pers.cmd;
	dist = (float)sqrt( distSq );
	inMelee = ( bot->client->ps.bitFlags & usingMelee ) ? qtrue : qfalse;

	BotLite_SetLockOn( bot, target );
	BotLite_FaceTarget( bot, target );
	BotLite_DebugLog( bot, "Combat tracking target" );

	/* Chase hard until PM_Melee actually latches. */
	cmd->forwardmove = 127;

	if ( dist > BOTLITE_MELEE_CHASE_DISTANCE && !inMelee ) {
		info->nextActionTime = 0;
		info->actionUntil = 0;
		return;
	}

	/* Inside real melee range, keep pressing forward until the engine puts us into usingMelee. */
	if ( !inMelee ) {
		if ( dist <= BOTLITE_MELEE_START_DISTANCE ) {
			info->nextActionTime = 0;
			info->actionUntil = 0;
		}
		return;
	}

	/* When frozen, maintain lock and let the engine recover. */
	if ( bot->client->ps.timers[tmFreeze] > 0 || bot->client->ps.timers[tmMeleeIdle] < 0 ) {
		return;
	}

	if ( info->nextActionTime == 0 && info->actionUntil == 0 ) {
		BotLite_StartCombo( clientNum );
	}

	if ( info->actionUntil > 0 ) {
		if ( level.time < info->actionUntil ) {
			BotLite_ApplyComboButtons( info, cmd );
			return;
		}

		info->actionUntil = 0;
		BotLite_AdvanceCombo( clientNum );
	}

	if ( level.time < info->nextActionTime ) {
		return;
	}

	/* Start current combo input and keep it held for the proper window. */
	if ( info->comboStage == BOTLITE_COMBO_PUNCH ) {
		cmd->buttons |= BUTTON_ATTACK;
		info->actionUntil = level.time + 90;
	}
	else if ( info->comboStage == BOTLITE_COMBO_KICK ) {
		cmd->buttons |= BUTTON_ALT_ATTACK;
		info->actionUntil = level.time + 90;
	}
	else if ( info->comboStage == BOTLITE_COMBO_SPEED ) {
		info->actionUntil = level.time + 240;
	}
	else {
		cmd->buttons |= BUTTON_ALT_ATTACK;
		info->actionUntil = level.time + 650;
	}
}

static void BotLite_RunCombatDefault( gentity_t *bot, gentity_t *target ) {
	BotLite_SetLockOn( bot, target );
	BotLite_FaceTarget( bot, target );
}


static void BotLite_RunCombatSkill2( gentity_t *bot, int clientNum, gentity_t *target, float distSq ) {
	botlite_info_t *info;
	usercmd_t *cmd;
	float dist;
	int weapon;
	qboolean inMelee;

	info = &g_botlite[clientNum];
	cmd = &bot->client->pers.cmd;
	dist = (float)sqrt( distSq );
	inMelee = ( bot->client->ps.bitFlags & usingMelee ) ? qtrue : qfalse;

	BotLite_SetLockOn( bot, target );
	BotLite_FaceTarget( bot, target );

	if ( inMelee ) {
		info->skill2ForceMelee = qtrue;
	}

	if ( info->skill2ForceMelee ) {
		if ( dist > BOTLITE_SKILL2_MELEE_EXIT_DISTANCE && !inMelee ) {
			info->skill2ForceMelee = qfalse;
		}
	}

	if ( !info->skill2ForceMelee && dist <= BOTLITE_SKILL2_RANGED_DISTANCE ) {
		info->skill2ForceMelee = qtrue;
	}

	if ( info->skill2ForceMelee ) {
		cmd->forwardmove = 127;
		cmd->buttons |= BUTTON_BOOST;
		BotLite_RunCombatSkill1( bot, clientNum, target, distSq );
		return;
	}

	if ( info->skill2Weapon <= 0 || info->skill2Weapon > MAX_PLAYERWEAPONS || level.time >= info->skill2WeaponSwitchTime ) {
		BotLite_StartSkill2Attack( bot, clientNum );
	}

	weapon = BotLite_PickSkill2Weapon( bot, info->skill2Weapon + 1 );
	info->skill2Weapon = weapon;
	cmd->weapon = weapon;

	if ( dist <= BOTLITE_SKILL2_APPROACH_TRIGGER_DISTANCE ) {
		if ( info->skill2ApproachTime <= level.time ) {
			info->skill2ApproachTime = level.time + BOTLITE_SKILL2_APPROACH_INTERVAL;
			info->actionUntil = level.time + BOTLITE_SKILL2_APPROACH_TIME;
		}
		if ( info->actionUntil > level.time ) {
			cmd->forwardmove = 127;
			cmd->buttons |= BUTTON_BOOST;
		}
	}

	if ( weapon == 1 ) {
		cmd->buttons |= BUTTON_ALT_ATTACK;
		return;
	}

	if ( bot->client->ps.weapon != weapon ) {
		return;
	}

	if ( bot->client->ps.weaponstate == WEAPON_ALTCHARGING || bot->client->ps.weaponstate == WEAPON_CHARGING ) {
		if ( bot->client->ps.stats[stChargePercentPrimary] >= BOTLITE_SKILL2_CHARGE_MIN ) {
			info->skill2AttackMode = 0;
			info->nextActionTime = level.time + 350;
			return;
		}
		cmd->buttons |= BUTTON_ATTACK;
		return;
	}

	if ( bot->client->ps.weaponstate == WEAPON_READY ) {
		if ( level.time < info->nextActionTime ) {
			return;
		}
		info->skill2AttackMode = 1;
		cmd->buttons |= BUTTON_ATTACK;
		return;
	}
}


static qboolean BotLite_Skill2BotDisallowsWeapon( gentity_t *bot, int weapon ) {
	char cleanName[MAX_NETNAME];

	if ( !bot || !bot->client ) {
		return qfalse;
	}

	Q_strncpyz( cleanName, bot->client->pers.netname, sizeof( cleanName ) );
	Q_CleanStr( cleanName );

	if ( Q_stricmp( cleanName, "goku" ) == 0 && weapon == 3 ) {
		return qtrue;
	}

	return qfalse;
}


static int BotLite_PickSkill2Weapon( gentity_t *bot, int currentWeapon ) {
	int i;
	int mask;
	int start;

	if ( !bot || !bot->client ) {
		return 1;
	}

	mask = bot->client->ps.stats[stSkills];
	start = currentWeapon;
	if ( start < 1 || start > MAX_PLAYERWEAPONS ) {
		start = 1;
	}

	for ( i = 0; i < MAX_PLAYERWEAPONS; i++ ) {
		int candidate;
		candidate = (( ( start - 1 ) + i ) % MAX_PLAYERWEAPONS) + 1;
		if ( mask & ( 1 << candidate ) ) {
			if ( BotLite_Skill2BotDisallowsWeapon( bot, candidate ) ) {
				continue;
			}
			return candidate;
		}
	}

	for ( i = 1; i <= MAX_PLAYERWEAPONS; i++ ) {
		if ( mask & ( 1 << i ) ) {
			if ( BotLite_Skill2BotDisallowsWeapon( bot, i ) ) {
				continue;
			}
			return i;
		}
	}

	return 1;
}


static void BotLite_StartSkill2Attack( gentity_t *bot, int clientNum ) {
	botlite_info_t *info;
	int weapon;

	info = &g_botlite[clientNum];
	weapon = BotLite_PickSkill2Weapon( bot, info->skill2Weapon );
	info->skill2Weapon = weapon;
	info->skill2WeaponSwitchTime = level.time + BOTLITE_SKILL2_WEAPON_SWITCH_TIME;
	info->skill2HoldUntil = 0;
			info->skill2ForceMelee = qfalse;

	if ( weapon == 1 ) {
		info->skill2AttackMode = 2;
		info->skill2HoldUntil = level.time + BOTLITE_SKILL2_WEAPON_SWITCH_TIME;
	}
	else {
		info->skill2AttackMode = 1;
	}
}

int BotLite_AddBot( const char *characterName, int skill, char *error, int errorSize ) {
	int clientNum;
	fileHandle_t f;
	char userinfo[MAX_INFO_STRING];
	const char *modelName;
	const char *teamName;
	char characterClean[MAX_QPATH];
	char physPath[MAX_QPATH];
	botlite_info_t *info;

	if ( error && errorSize > 0 ) {
		error[0] = '\0';
	}

	if ( !characterName || !characterName[0] ) {
		if ( error ) {
			Q_strncpyz( error, "Usage: /addbot <character> [skill 1-3]", errorSize );
		}
		return -1;
	}

	Q_strncpyz( characterClean, characterName, sizeof( characterClean ) );
	modelName = characterClean;

	Com_sprintf( physPath, sizeof( physPath ), "players/%s/default.phys", modelName );
	if ( trap_FS_FOpenFile( physPath, &f, FS_READ ) <= 0 ) {
		if ( error ) {
			Com_sprintf( error, errorSize, "Character '%s' was not found.", modelName );
		}
		return -1;
	}
	trap_FS_FCloseFile( f );

	clientNum = trap_BotAllocateClient();
	if ( clientNum < 0 ) {
		if ( error ) {
			Q_strncpyz( error, "No free client slots available for a bot.", errorSize );
		}
		return -1;
	}

	memset( userinfo, 0, sizeof( userinfo ) );
	Info_SetValueForKey( userinfo, "name", modelName );
	Info_SetValueForKey( userinfo, "rate", "25000" );
	Info_SetValueForKey( userinfo, "snaps", "20" );
	Info_SetValueForKey( userinfo, "ip", "localhost" );
	Info_SetValueForKey( userinfo, "cg_predictItems", "0" );
	Info_SetValueForKey( userinfo, "cg_advancedFlight", "1" );
	Info_SetValueForKey( userinfo, "handicap", "100" );
	Info_SetValueForKey( userinfo, "model", modelName );
	Info_SetValueForKey( userinfo, "headmodel", modelName );
	Info_SetValueForKey( userinfo, "legsmodel", modelName );
	Info_SetValueForKey( userinfo, "color1", "4" );
	Info_SetValueForKey( userinfo, "color2", "4" );
	Info_SetValueForKey( userinfo, "skill", va( "%d", BotLite_ClampSkill( skill ) ) );

	if ( g_gametype.integer >= GT_TEAM ) {
		team_t team;
		team = PickTeam( clientNum );
		teamName = ( team == TEAM_BLUE ) ? "blue" : "red";
		Info_SetValueForKey( userinfo, "team", teamName );
	}

	trap_SetUserinfo( clientNum, userinfo );
	g_entities[clientNum].r.svFlags |= SVF_BOT;

	if ( ClientConnect( clientNum, qtrue, qtrue ) ) {
		trap_BotFreeClient( clientNum );
		if ( error ) {
			Q_strncpyz( error, "ClientConnect rejected the bot.", errorSize );
		}
		return -1;
	}

	ClientBegin( clientNum );
	info = &g_botlite[clientNum];
	memset( info, 0, sizeof( *info ) );
	info->lastLoggedMode = -1;
	info->inuse = qtrue;
	info->skill = BotLite_ClampSkill( skill );
	info->mode = ( info->skill == 1 || info->skill == 2 ) ? BOTLITE_MODE_SEARCH : BOTLITE_MODE_IDLE;
	info->lastTargetNum = -1;
	info->skill2Weapon = 1;
	info->skill2AttackMode = 0;
	info->skill2WeaponSwitchTime = 0;
	info->skill2HoldUntil = 0;
	info->skill2ApproachTime = 0;
	info->retreatUntil = 0;
	info->retreatYaw = 0.0f;
	info->postCrashFlyup = qfalse;
	info->postCrashRiseEndTime = 0;
			info->skill2ForceMelee = qfalse;
	info->skill2ForceMelee = qfalse;
	Q_strncpyz( info->character, modelName, sizeof( info->character ) );

	return clientNum;
}


int BotLite_AddBotDebug( const char *characterName, int skill, char *error, int errorSize ) {
	int clientNum;

	clientNum = BotLite_AddBot( characterName, skill, error, errorSize );
	if ( clientNum >= 0 ) {
		g_botlite[clientNum].debugEnabled = qtrue;
		g_botlite[clientNum].lastLoggedMode = -1;
		BotLite_DebugLog( &g_entities[clientNum], "Debug enabled" );
	}
	return clientNum;
}

int BotLite_RemoveBotByName( const char *characterName, char *error, int errorSize ) {
	int i;
	char cleanArg[MAX_QPATH];

	if ( error && errorSize > 0 ) {
		error[0] = '\0';
	}

	if ( !characterName || !characterName[0] ) {
		if ( error ) {
			Q_strncpyz( error, "Usage: /removebot <character>", errorSize );
		}
		return -1;
	}

	Q_strncpyz( cleanArg, characterName, sizeof( cleanArg ) );
	Q_CleanStr( cleanArg );

	for ( i = 0; i < level.maxclients; i++ ) {
		char cleanBotName[MAX_QPATH];
		gentity_t *ent;

		if ( !BotLite_IsManagedBot( i ) ) {
			continue;
		}

		ent = &g_entities[i];
		Q_strncpyz( cleanBotName, g_botlite[i].character, sizeof( cleanBotName ) );
		Q_CleanStr( cleanBotName );
		if ( Q_stricmp( cleanBotName, cleanArg ) != 0 ) {
			continue;
		}

		BotLite_ClearLock( ent );
		trap_DropClient( i, "bot removed" );
		return i;
	}

	if ( error ) {
		Com_sprintf( error, errorSize, "Bot '%s' was not found.", characterName );
	}
	return -1;
}

int BotLite_RemoveAllBots( void ) {
	int i;
	int removed;

	removed = 0;
	for ( i = 0; i < level.maxclients; i++ ) {
		if ( !BotLite_IsManagedBot( i ) ) {
			continue;
		}
		BotLite_ClearLock( &g_entities[i] );
		trap_DropClient( i, "all bots removed" );
		removed++;
	}
	return removed;
}

int BotAISetup( int restart ) {
	return 0;
}

int BotAIShutdown( int restart ) {
	return 0;
}

int BotAILoadMap( int restart ) {
	return 0;
}

int BotAISetupClient( int client, struct bot_settings_s *settings, qboolean restart ) {
	return 0;
}

int BotAIShutdownClient( int client, qboolean restart ) {
	BotLite_OnClientDisconnect( client );
	return 0;
}

int BotAIStartFrame( int time ) {
	int i;

	for ( i = 0; i < level.maxclients; i++ ) {
		gentity_t *ent;
		gentity_t *target;
		usercmd_t *cmd;
		float distSq;
		botlite_info_t *info;

		if ( !BotLite_IsManagedBot( i ) ) {
			continue;
		}

		ent = &g_entities[i];
		if ( !ent->inuse || !ent->client || !( ent->r.svFlags & SVF_BOT ) ) {
			continue;
		}
		if ( ent->client->pers.connected != CON_CONNECTED ) {
			continue;
		}
		if ( ent->client->sess.sessionTeam == TEAM_SPECTATOR ) {
			continue;
		}

		cmd = &ent->client->pers.cmd;
		memset( cmd, 0, sizeof( *cmd ) );
		cmd->serverTime = time;

		if ( ent->client->ps.bitFlags & isDead ) {
			info = &g_botlite[i];
			info->mode = ( info->skill == 1 || info->skill == 2 ) ? BOTLITE_MODE_SEARCH : BOTLITE_MODE_IDLE;
			info->didInitialRise = qfalse;
							info->lastTargetNum = -1;
			info->nextActionTime = 0;
			info->actionUntil = 0;
			info->skill2WeaponSwitchTime = 0;
			info->skill2HoldUntil = 0;
			info->skill2ApproachTime = 0;
			info->retreatUntil = 0;
			info->retreatYaw = 0.0f;
			info->postCrashFlyup = qfalse;
			info->postCrashRiseEndTime = 0;
			BotLite_ClearLock( ent );
			continue;
		}

		info = &g_botlite[i];
		if ( BotLite_IsTemporarilyDisabled( ent ) ) {
            info->didInitialRise = qfalse;
            if ( ( ent->client->ps.bitFlags & isCrashed ) ||
                 ( ent->client->ps.bitFlags & isUnconcious ) ||
                 ent->client->ps.timers[tmCrash] != 0 ||
                 ent->client->ps.timers[tmRecover] != 0 ||
                 ent->client->ps.powerups[PW_STATE] == -1 ) {
                info->postCrashFlyup = qtrue;
            }
			cmd->forwardmove = 0;
			cmd->rightmove = 0;
			cmd->upmove = 0;
			cmd->buttons = 0;
			info->nextActionTime = 0;
			info->actionUntil = 0;
			info->comboStep = 0;
			info->skill2HoldUntil = 0;
			continue;
		}
		if ( info->lastLoggedMode != info->mode ) {
			BotLite_DebugLog( ent, "Mode changed" );
			info->lastLoggedMode = info->mode;
		}
		if ( BotLite_RunRecoveryFlyup( ent, i ) ) {
			continue;
		}
		if ( BotLite_RunPostCrashFlyup( ent, i ) ) {
			continue;
		}
		target = NULL;
		distSq = BOTLITE_LOCK_RANGE * BOTLITE_LOCK_RANGE;
		if ( info->mode == BOTLITE_MODE_WAIT_TARGET_RECOVERY ) {
			target = BotLite_GetTrackedTarget( ent, i, &distSq );
			if ( BotLite_RunRecoveryWait( ent, i, target ) ) {
				continue;
			}
			target = NULL;
			distSq = BOTLITE_LOCK_RANGE * BOTLITE_LOCK_RANGE;
		}

		if ( info->skill == 1 && BotLite_ReactToDamage( ent, i, &distSq ) ) {
			target = BotLite_GetTrackedTarget( ent, i, &distSq );
		}

		if ( !target ) {
			if ( info->skill == 2 ) {
				target = BotLite_FindNearestPlayerAnyDistance( ent, &distSq );
				if ( target ) {
					info->lastTargetNum = target->s.number;
					info->mode = BOTLITE_MODE_COMBAT;
				}
			}
			else if ( info->mode == BOTLITE_MODE_COMBAT ) {
				target = BotLite_GetTrackedTarget( ent, i, &distSq );
			} else {
				target = BotLite_FindNearestVisiblePlayer( ent, &distSq );
				if ( target ) {
					info->lastTargetNum = target->s.number;
				}
			}
		}
		if ( info->mode == BOTLITE_MODE_SEARCH ) {
			if ( info->skill == 1 ) {
				BotLite_RunSearchSkill1( ent, i, target );
			} else {
				BotLite_RunSearchDefault( ent, i, target );
			}
		}
		else if ( info->mode == BOTLITE_MODE_COMBAT ) {
			if ( !target ) {
				BotLite_ClearLock( ent );
				info->mode = ( info->skill == 1 || info->skill == 2 ) ? BOTLITE_MODE_SEARCH : BOTLITE_MODE_IDLE;
				info->lastTargetNum = -1;
				info->nextActionTime = 0;
				info->actionUntil = 0;
				continue;
			}
			if ( BotLite_TargetNeedsRecoveryWait( target ) ) {
				BotLite_ClearLock( ent );
				BotLite_StartRecoveryWait( ent, i, target );
				if ( BotLite_RunRecoveryWait( ent, i, target ) ) {
					continue;
				}
			}
			if ( info->skill == 1 ) {
				BotLite_RunCombatSkill1( ent, i, target, distSq );
			} else if ( info->skill == 2 ) {
				BotLite_RunCombatSkill2( ent, i, target, distSq );
			} else {
				BotLite_RunCombatDefault( ent, target );
			}
		}
		else if ( target ) {
			if ( info->skill == 2 ) {
				info->mode = BOTLITE_MODE_COMBAT;
				BotLite_RunCombatSkill2( ent, i, target, distSq );
			} else {
				BotLite_FaceTarget( ent, target );
			}
		}
	}

	return 0;
}
