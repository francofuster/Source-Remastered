#include "g_local.h"
#include "g_botlite.h"

static float BotLite_RandomRange( float minv, float maxv );
static float BotLite_AngleNormalize360( float angle );
static float BotLite_ShortestAngleDelta( float from, float to );
static void BotLite_SetMoveYaw( gentity_t *bot, int clientNum );
static void BotLite_MoveSearchPattern( gentity_t *bot, int clientNum );

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


qboolean BotLite_RunPostCrashFlyup( gentity_t *bot, int clientNum ) {
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

	BotLite_DebugLog( bot, va( "Finish post-crash flyup lastTarget=%d", info->lastTargetNum ) );
	info->postCrashFlyup = qfalse;
	info->postCrashRiseEndTime = 0;
	info->didInitialRise = qtrue;
	info->mode = ( info->lastTargetNum >= 0 ) ? BOTLITE_MODE_COMBAT : BOTLITE_MODE_SEARCH;
	info->nextActionTime = 0;
	info->actionUntil = 0;
	return qfalse;
}


void BotLite_StartRecoveryWait( gentity_t *bot, int clientNum, gentity_t *target ) {
	botlite_info_t *info;
	vec3_t delta;

	info = &g_botlite[clientNum];
	info->mode = BOTLITE_MODE_WAIT_TARGET_RECOVERY;
	info->lastTargetNum = target ? target->s.number : -1;
	info->nextActionTime = 0;
	info->actionUntil = 0;
	info->comboStep = 0;
	info->retreatUntil = level.time + 3000;
	info->recoveryWaitEndTime = 0;
	BotLite_DebugLog( bot, va( "Start retreat target=%d retreatUntil=%d", info->lastTargetNum, info->retreatUntil ) );
	
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


qboolean BotLite_RunRecoveryWait( gentity_t *bot, int clientNum, gentity_t *target ) {
	botlite_info_t *info;
	usercmd_t *cmd;
	vec3_t angles;
	qboolean targetNeedsRecovery;

	info = &g_botlite[clientNum];
	if ( info->mode != BOTLITE_MODE_WAIT_TARGET_RECOVERY ) {
		return qfalse;
	}

	cmd = &bot->client->pers.cmd;
	targetNeedsRecovery = ( target && target->client ) ? BotLite_TargetNeedsRecoveryWait( target ) : qfalse;

	if ( level.time < info->retreatUntil ) {
		VectorClear( angles );
		angles[YAW] = info->retreatYaw;
		BotLite_ApplyViewAngles( bot, angles );

		cmd->forwardmove = 127;
		cmd->buttons |= BUTTON_BOOST;
		return qtrue;
	}

	if ( info->retreatUntil != 0 ) {
		BotLite_DebugLog( bot, va( "Retreat finished, target=%d recovered=%d", info->lastTargetNum, targetNeedsRecovery ? 0 : 1 ) );
		info->retreatUntil = 0;
	}

	if ( target && target->client && targetNeedsRecovery ) {
		BotLite_FaceTarget( bot, target );
		cmd->forwardmove = 0;
		cmd->rightmove = 0;
		cmd->upmove = 0;
		cmd->buttons = 0;
		return qtrue;
	}

	BotLite_DebugLog( bot, va( "Recovery finished -> reengage target=%d", info->lastTargetNum ) );
	info->mode = BOTLITE_MODE_COMBAT;
	info->nextActionTime = 0;
	info->actionUntil = 0;
	info->comboStep = 0;
	info->retreatUntil = 0;
	info->recoveryWaitEndTime = 0;

	if ( target && target->client ) {
		info->lastTargetNum = target->s.number;
		BotLite_SetLockOn( bot, target );
	} else {
		info->lastTargetNum = -1;
		info->mode = ( info->skill == 1 || info->skill == 2 ) ? BOTLITE_MODE_SEARCH : BOTLITE_MODE_IDLE;
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


void BotLite_RunSearchSkill1( gentity_t *bot, int clientNum, gentity_t *target ) {
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


void BotLite_RunSearchDefault( gentity_t *bot, int clientNum, gentity_t *target ) {
	if ( target ) {
		g_botlite[clientNum].mode = BOTLITE_MODE_COMBAT;
		g_botlite[clientNum].lastTargetNum = target->s.number;
		BotLite_DebugLog( bot, "Default search acquired target" );
		BotLite_SetLockOn( bot, target );
	}
}


