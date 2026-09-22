#include "g_local.h"
#include "g_botlite.h"

static void BotLite_FailsafeRecoverCrash( gentity_t *bot, botlite_info_t *info );
static gentity_t *BotLite_GetLastAttacker( gentity_t *bot );
static gentity_t *BotLite_FindBestDamageAttacker( gentity_t *bot, float *outDistSq );
static void BotLite_FaceDamageDirection( gentity_t *bot );
static qboolean BotLite_HasLineOfSight( gentity_t *bot, gentity_t *target );
static qboolean BotLite_TargetFacingBot( gentity_t *bot, gentity_t *target );
static qboolean BotLite_TargetChargingAttack( gentity_t *target );
static qboolean BotLite_SnapshotBotDisabled( gentity_t *bot );

void BotLite_DebugLog( gentity_t *bot, const char *msg ) {
	botlite_info_t *info;

	if ( !bot || !bot->client ) {
		return;
	}

	info = &g_botlite[bot->s.number];
	if ( !info->runtime.debugEnabled ) {
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
		 bot->client->ps.timers[tmCrash] <= 0 &&
		 bot->client->ps.timers[tmRecover] <= 0 &&
		 bot->client->ps.timers[tmKnockback] <= 0 ) {
		info->runtime.crashStartTime = 0;
		return;
	}

	if ( info->runtime.crashStartTime <= 0 ) {
		info->runtime.crashStartTime = level.time;
		BotLite_DebugLog( bot, "Crash state started" );
		return;
	}

	if ( level.time - info->runtime.crashStartTime < 3000 ) {
		return;
	}

	bot->client->ps.bitFlags &= ~isCrashed;
	bot->client->ps.bitFlags &= ~isUnconcious;
	bot->client->ps.powerups[PW_STATE] = 0;
	bot->client->ps.timers[tmCrash] = 0;
	bot->client->ps.timers[tmRecover] = 0;
	bot->client->ps.timers[tmKnockback] = 0;
	BotLite_ResetTransientCombatState( bot->s.number );
	info->runtime.crashStartTime = 0;

	BotLite_DebugLog( bot, "Failsafe recovery applied" );
}

qboolean BotLite_IsTemporarilyDisabled( gentity_t *bot ) {
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

qboolean BotLite_TargetIsValid( gentity_t *bot, gentity_t *target ) {
	qboolean targetIsBot;

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
	targetIsBot = ( target->r.svFlags & SVF_BOT ) ? qtrue : qfalse;
	if ( targetIsBot && !g_botlite_targetBots.integer ) {
		BotLite_DebugLog( bot, va( "Reject bot target=%d name=%s reason=targetBotsOff", target->s.number, target->client->pers.netname ) );
		return qfalse;
	}
	if ( target->client->ps.bitFlags & isDead ) {
		return qfalse;
	}
	return qtrue;
}

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

qboolean BotLite_ReactToDamage( gentity_t *bot, int clientNum, float *outDistSq ) {
	botlite_info_t *info;
	gentity_t *attacker;
	vec3_t delta;
	float distSq;

	info = &g_botlite[clientNum];
	if ( bot->client->ps.damageEvent == info->runtime.lastDamageEvent || bot->client->ps.damageCount <= 0 ) {
		return qfalse;
	}

	info->runtime.lastDamageEvent = bot->client->ps.damageEvent;
	BotLite_DebugLog( bot, va( "Received damage dmgEv=%d count=%d lastHurt=%d persAttacker=%d", bot->client->ps.damageEvent, bot->client->ps.damageCount, bot->client->lasthurt_client, bot->client->ps.persistant[PERS_ATTACKER] ) );
	info->runtime.mode = BOTLITE_MODE_COMBAT;
	BotLite_ResetTransientCombatState( clientNum );

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
		info->runtime.lastTargetNum = attacker->s.number;
		BotLite_DebugLog( bot, "Damage reaction target acquired" );
		BotLite_SetLockOn( bot, attacker );
		BotLite_FaceTarget( bot, attacker );
		if ( outDistSq ) {
			*outDistSq = distSq;
		}
		return qtrue;
	}

	info->runtime.lastTargetNum = -1;
	BotLite_FaceDamageDirection( bot );
	if ( outDistSq ) {
		*outDistSq = info->profile ? info->profile->lockRange * info->profile->lockRange : ( 2200.0f * 2200.0f );
	}
	return qtrue;
}

gentity_t *BotLite_GetTrackedTarget( gentity_t *bot, int clientNum, float *outDistSq ) {
	botlite_info_t *info;
	gentity_t *target;
	vec3_t delta;
	float distSq;

	info = &g_botlite[clientNum];
	target = NULL;
	distSq = info->profile ? info->profile->lockRange * info->profile->lockRange : ( 2200.0f * 2200.0f );

	if ( info->runtime.lastTargetNum >= 0 && info->runtime.lastTargetNum < level.maxclients ) {
		target = &g_entities[info->runtime.lastTargetNum];
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
			info->runtime.lastTargetNum = target->s.number;
		}
	}

	if ( outDistSq ) {
		*outDistSq = distSq;
	}
	return target;
}

gentity_t *BotLite_FindNearestPlayerAnyDistance( gentity_t *bot, float *outDistSq ) {
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
		*outDistSq = best ? bestDistSq : 0.0f;
	}
	return best;
}

gentity_t *BotLite_FindNearestVisiblePlayer( gentity_t *bot, float *outDistSq ) {
	int i;
	gentity_t *best;
	float bestDistSq;
	float minDistSq;
	float maxDistSq;
	const botlite_profile_t *profile;
	botlite_info_t *info;
	qboolean requireLOS;

	best = NULL;
	bestDistSq = 999999999.0f;
	minDistSq = 0.0f;
	maxDistSq = 0.0f;
	requireLOS = qfalse;

	profile = NULL;
	info = NULL;
	if ( bot && bot->s.number >= 0 && bot->s.number < BOTLITE_MAX_BOTS ) {
		info = &g_botlite[bot->s.number];
		profile = info->profile ? info->profile : BotLite_GetProfile( info->skill );
	}
	if ( profile ) {
		if ( profile->targetAcquireMinDistance > 0.0f ) {
			minDistSq = profile->targetAcquireMinDistance * profile->targetAcquireMinDistance;
		}
		if ( profile->acquireDistance > 0.0f ) {
			maxDistSq = profile->acquireDistance * profile->acquireDistance;
		}
		requireLOS = profile->targetAcquireRequiresLOS;
	}

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
		if ( distSq < minDistSq ) {
			continue;
		}
		if ( maxDistSq > 0.0f && distSq > maxDistSq ) {
			continue;
		}
		if ( requireLOS && !BotLite_HasLineOfSight( bot, other ) ) {
			continue;
		}
		if ( distSq >= bestDistSq ) {
			continue;
		}

		best = other;
		bestDistSq = distSq;
	}

	if ( outDistSq ) {
		*outDistSq = best ? bestDistSq : 0.0f;
	}
	return best;
}

qboolean BotLite_TargetStillRecovering( gentity_t *target ) {
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

qboolean BotLite_TargetNeedsRecoveryWait( gentity_t *target ) {
	return BotLite_TargetStillRecovering( target );
}

static qboolean BotLite_HasLineOfSight( gentity_t *bot, gentity_t *target ) {
	trace_t tr;
	vec3_t start;
	vec3_t end;

	if ( !bot || !bot->client || !target || !target->client ) {
		return qfalse;
	}

	VectorCopy( bot->client->ps.origin, start );
	start[2] += BOTLITE_VIEW_HEIGHT;
	VectorCopy( target->client->ps.origin, end );
	end[2] += BOTLITE_VIEW_HEIGHT;
	trap_Trace( &tr, start, NULL, NULL, end, bot->s.number, MASK_SHOT );
	if ( tr.fraction >= 1.0f ) {
		return qtrue;
	}
	if ( tr.entityNum == target->s.number ) {
		return qtrue;
	}
	return qfalse;
}

static qboolean BotLite_TargetFacingBot( gentity_t *bot, gentity_t *target ) {
	vec3_t delta;
	vec3_t targetAngles;
	float yawToBot;
	float yawDelta;

	if ( !bot || !target || !target->client ) {
		return qfalse;
	}

	VectorSubtract( bot->client->ps.origin, target->client->ps.origin, delta );
	yawToBot = vectoyaw( delta );
	VectorCopy( target->client->ps.viewangles, targetAngles );
	yawDelta = fabs( BotLite_ShortestAngleDelta( targetAngles[YAW], yawToBot ) );
	return ( yawDelta <= 45.0f ) ? qtrue : qfalse;
}

static qboolean BotLite_TargetChargingAttack( gentity_t *target ) {
	if ( !target || !target->client ) {
		return qfalse;
	}

	if ( target->client->ps.weaponstate == WEAPON_CHARGING ||
		 target->client->ps.weaponstate == WEAPON_ALTCHARGING ||
		 target->client->ps.weaponstate == WEAPON_GUIDING ||
		 target->client->ps.weaponstate == WEAPON_ALTGUIDING ) {
		return qtrue;
	}

	return qfalse;
}

static qboolean BotLite_SnapshotBotDisabled( gentity_t *bot ) {
	if ( !bot || !bot->client ) {
		return qtrue;
	}
	if ( ( bot->client->ps.bitFlags & isCrashed ) ||
		 ( bot->client->ps.bitFlags & isUnconcious ) ||
		 bot->client->ps.timers[tmCrash] > 0 ||
		 bot->client->ps.timers[tmKnockback] > 0 ||
		 bot->client->ps.timers[tmRecover] > 0 ) {
		return qtrue;
	}
	return qfalse;
}

void BotLite_PopulateSnapshotMetrics( gentity_t *bot, int clientNum, gentity_t *target, float distSq, botlite_snapshot_t *snapshot ) {
	vec3_t delta;

	if ( !snapshot ) {
		return;
	}

	snapshot->bot = bot;
	snapshot->botActionFlags = ( bot && bot->client && clientNum >= 0 && clientNum < level.maxclients ) ? trap_BotQueryActionState( clientNum ) : 0;
	snapshot->botInMelee = ( snapshot->botActionFlags & BOTACT_USING_MELEE ) ? qtrue : qfalse;
	snapshot->botFrozen = ( snapshot->botActionFlags & ( BOTACT_FREEZE | BOTACT_MELEE_RECOVERY ) ) ? qtrue : qfalse;
	snapshot->botDisabled = ( snapshot->botActionFlags & ( BOTACT_RECOVERING | BOTACT_KNOCKBACK | BOTACT_CRASHED | BOTACT_UNCONSCIOUS ) ) ? qtrue : BotLite_SnapshotBotDisabled( bot );

	/* El estado propio es valido aunque no haya objetivo: el presupuesto de recursos
	 * debe poder consultarse tambien mientras busca o se recupera. */
	if ( bot && bot->client ) {
		snapshot->botMeleeState = bot->client->ps.stats[stMeleeState];
		snapshot->botMeleeChargeTime = bot->client->ps.timers[tmMeleeCharge];
		snapshot->botKnockbackTime = bot->client->ps.timers[tmKnockback];
		snapshot->botBoostTime = bot->client->ps.timers[tmBoost];
		snapshot->botStruggling = ( bot->client->ps.bitFlags & isStruggling ) ? qtrue : qfalse;
		snapshot->botFatigue = bot->client->ps.powerLevel[plFatigue];
		snapshot->botFatigueMax = bot->client->ps.powerLevel[plMaximum];
		snapshot->botKi = bot->client->ps.powerLevel[plCurrent];
		snapshot->botKiMax = bot->client->ps.powerLevel[plMaximum];
		snapshot->botHealth = bot->client->ps.powerLevel[plHealth];
		snapshot->botTier = bot->client->ps.powerLevel[plTierCurrent];
	}

	if ( !bot || !bot->client || !target || !target->client ) {
		return;
	}

	VectorSubtract( target->client->ps.origin, bot->client->ps.origin, delta );
	snapshot->distSq = distSq;
	snapshot->dist = sqrt( distSq );
	snapshot->horizontalDist = sqrt( delta[0] * delta[0] + delta[1] * delta[1] );
	snapshot->verticalDelta = delta[2];
	snapshot->hasLineOfSight = BotLite_HasLineOfSight( bot, target );
	snapshot->targetFacingBot = BotLite_TargetFacingBot( bot, target );
	snapshot->targetCharging = BotLite_TargetChargingAttack( target );
	snapshot->targetBlocking = ( target->client->ps.bitFlags & usingBlock ) ? qtrue : qfalse;
	snapshot->targetInMelee = ( target->client->ps.bitFlags & usingMelee ) ? qtrue : qfalse;
	snapshot->targetWeapon = target->client->ps.weapon;
	snapshot->targetMeleeState = target->client->ps.stats[stMeleeState];
	snapshot->targetMeleeChargeTime = target->client->ps.timers[tmMeleeCharge];
	snapshot->targetKnockbackTime = target->client->ps.timers[tmKnockback];
	snapshot->targetStruggling = ( target->client->ps.bitFlags & isStruggling ) ? qtrue : qfalse;
	snapshot->targetFatigue = target->client->ps.powerLevel[plFatigue];
	snapshot->targetKi = target->client->ps.powerLevel[plCurrent];
	snapshot->targetKiMax = target->client->ps.powerLevel[plMaximum];
	snapshot->targetHealth = target->client->ps.powerLevel[plHealth];
	snapshot->targetTier = target->client->ps.powerLevel[plTierCurrent];
}

const char *BotLite_MeleeStateName( int meleeState ) {
	switch ( meleeState ) {
	case stMeleeInactive: return "INACTIVE";
	case stMeleeAggressing: return "AGGRESS";
	case stMeleeDegressing: return "DEGRESS";
	case stMeleeIdle: return "IDLE";
	case stMeleeStartPower: return "START_POWER";
	case stMeleeStartAttack: return "START_ATTACK";
	case stMeleeStartDodge: return "START_DODGE";
	case stMeleeStartHit: return "START_HIT";
	case stMeleeUsingSpeed: return "SPEED";
	case stMeleeUsingPower: return "POWER";
	case stMeleeUsingStun: return "STUN";
	case stMeleeUsingBlock: return "BLOCK";
	case stMeleeUsingEvade: return "EVADE";
	case stMeleeUsingSpeedBreaker: return "SPEED_BREAKER";
	case stMeleeUsingChargeBreaker: return "CHARGE_BREAKER";
	case stMeleeUsingZanzoken: return "ZANZOKEN";
	case stMeleeChargingPower: return "CHARGING_POWER";
	case stMeleeChargingStun: return "CHARGING_STUN";
	default: return "?";
	}
}

/* Volcado periodico del snapshot: es el criterio de aceptacion de T0.1 y la
 * herramienta para depurar todas las tareas reactivas que vienen despues. */
void BotLite_DebugLogSnapshot( gentity_t *bot, int clientNum, const botlite_snapshot_t *snapshot ) {
	botlite_info_t *info;
	int fatiguePct;
	int kiPct;

	if ( !bot || !bot->client || !snapshot ) {
		return;
	}
	if ( clientNum < 0 || clientNum >= level.maxclients ) {
		return;
	}

	info = &g_botlite[clientNum];
	if ( !info->runtime.debugEnabled ) {
		return;
	}
	if ( level.time < info->runtime.snapshotDiagNextLogTime ) {
		return;
	}
	info->runtime.snapshotDiagNextLogTime = level.time + 1000;

	fatiguePct = ( snapshot->botFatigueMax > 0 ) ? (int)( ( (float)snapshot->botFatigue * 100.0f ) / (float)snapshot->botFatigueMax ) : 0;
	kiPct = ( snapshot->botKiMax > 0 ) ? (int)( ( (float)snapshot->botKi * 100.0f ) / (float)snapshot->botKiMax ) : 0;

	BotLite_DebugLog( bot, va( "SNAP self melee=%s chg=%d kb=%d strug=%d fatigue=%d%%(%s) ki=%d%% hp=%d tier=%d",
		BotLite_MeleeStateName( snapshot->botMeleeState ),
		snapshot->botMeleeChargeTime,
		snapshot->botKnockbackTime,
		snapshot->botStruggling ? 1 : 0,
		fatiguePct,
		BotLite_StaminaBudgetName( BotLite_StaminaBudget( clientNum ) ),
		kiPct,
		snapshot->botHealth,
		snapshot->botTier ) );

	if ( !snapshot->hasTarget ) {
		BotLite_DebugLog( bot, "SNAP target none" );
		return;
	}

	BotLite_DebugLog( bot, va( "SNAP targ melee=%s chg=%d kb=%d strug=%d ki=%d hp=%d tier=%d dist=%d los=%d",
		BotLite_MeleeStateName( snapshot->targetMeleeState ),
		snapshot->targetMeleeChargeTime,
		snapshot->targetKnockbackTime,
		snapshot->targetStruggling ? 1 : 0,
		snapshot->targetKi,
		snapshot->targetHealth,
		snapshot->targetTier,
		(int)snapshot->dist,
		snapshot->hasLineOfSight ? 1 : 0 ) );
}

void BotLite_UpdateTargetRecoveryState( gentity_t *bot, int clientNum, gentity_t *target, float distSq, botlite_snapshot_t *snapshot ) {
	botlite_info_t *info;
	qboolean targetCrashNow;
	qboolean targetCrashPartial;
	qboolean targetCrashEdge;

	if ( !snapshot ) {
		return;
	}

	info = &g_botlite[clientNum];
	targetCrashEdge = qfalse;
	targetCrashNow = qfalse;
	targetCrashPartial = qfalse;

	if ( target && target->client ) {
		if ( info->runtime.targetRecoveryHandledNum != target->s.number ) {
			info->runtime.targetRecoveryHandled = qfalse;
			info->runtime.targetRecoveryHandledNum = target->s.number;
			info->runtime.targetRecoveryHandledEvent = target->client->botCrashEventCounter;
			info->runtime.targetCrashActive = qfalse;
			info->runtime.crashDiagNextLogTime = 0;
			BotLite_DebugLog( bot, va( "Target switch sync target=%d crashCounter=%d", target->s.number, target->client->botCrashEventCounter ) );
		}
		targetCrashNow = BotLite_TargetNeedsRecoveryWait( target );
		targetCrashPartial = ( target->client->ps.timers[tmKnockback] > 0 ) ||
			( target->client->ps.timers[tmCrash] > 0 ) ||
			( target->client->ps.timers[tmRecover] > 0 ) ||
			( target->client->ps.powerups[PW_STATE] == -1 ) ||
			( target->client->ps.bitFlags & isCrashed ) ||
			( target->client->ps.bitFlags & isUnconcious );

		if ( targetCrashNow && !info->runtime.targetCrashActive ) {
			targetCrashEdge = qtrue;
		} else if ( target->client->botCrashEventCounter != info->runtime.targetRecoveryHandledEvent ) {
			targetCrashEdge = qtrue;
		}

		info->runtime.targetCrashActive = targetCrashNow;
	} else if ( info->runtime.mode != BOTLITE_MODE_WAIT_TARGET_RECOVERY ) {
		info->runtime.targetRecoveryHandled = qfalse;
		info->runtime.targetRecoveryHandledNum = -1;
		info->runtime.targetRecoveryHandledEvent = -1;
		info->runtime.crashDiagNextLogTime = 0;
		info->runtime.targetCrashActive = qfalse;
	}

	snapshot->targetCrashNow = targetCrashNow;
	snapshot->targetCrashEdge = targetCrashEdge;
	snapshot->targetCrashPartial = targetCrashPartial;
}
