#include "g_local.h"
#include "g_botlite.h"

static void BotLite_FailsafeRecoverCrash( gentity_t *bot, botlite_info_t *info );
static gentity_t *BotLite_GetLastAttacker( gentity_t *bot );
static gentity_t *BotLite_FindBestDamageAttacker( gentity_t *bot, float *outDistSq );
static void BotLite_FaceDamageDirection( gentity_t *bot );

void BotLite_DebugLog( gentity_t *bot, const char *msg ) {
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
		 bot->client->ps.timers[tmCrash] <= 0 &&
		 bot->client->ps.timers[tmRecover] <= 0 &&
		 bot->client->ps.timers[tmKnockback] <= 0 ) {
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
		if ( targetIsBot ) {
			BotLite_DebugLog( bot, va( "Reject bot target=%d name=%s reason=dead", target->s.number, target->client->pers.netname ) );
		}
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
	if ( bot->client->ps.damageEvent == info->lastDamageEvent || bot->client->ps.damageCount <= 0 ) {
		return qfalse;
	}

	info->lastDamageEvent = bot->client->ps.damageEvent;
	BotLite_DebugLog( bot, va( "Received damage dmgEv=%d count=%d lastHurt=%d persAttacker=%d", bot->client->ps.damageEvent, bot->client->ps.damageCount, bot->client->lasthurt_client, bot->client->ps.persistant[PERS_ATTACKER] ) );
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


gentity_t *BotLite_GetTrackedTarget( gentity_t *bot, int clientNum, float *outDistSq ) {
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

	if ( best && ( best->r.svFlags & SVF_BOT ) ) {
		BotLite_DebugLog( bot, va( "AnyDistance picked bot target=%d name=%s dist=%.0f", best->s.number, best->client->pers.netname, (float)sqrt( bestDistSq ) ) );
	}

	if ( outDistSq ) {
		*outDistSq = best ? bestDistSq : ( BOTLITE_LOCK_RANGE * BOTLITE_LOCK_RANGE );
	}
	return best;
}


gentity_t *BotLite_FindNearestVisiblePlayer( gentity_t *bot, float *outDistSq ) {
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

	if ( best && ( best->r.svFlags & SVF_BOT ) ) {
		BotLite_DebugLog( bot, va( "Visible picked bot target=%d name=%s dist=%.0f", best->s.number, best->client->pers.netname, (float)sqrt( bestDistSq ) ) );
	}

	if ( outDistSq ) {
		*outDistSq = bestDistSq;
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


