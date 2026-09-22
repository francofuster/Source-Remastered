#include "g_local.h"
#include "g_botlite.h"

qboolean BotLite_IsTargetDead( gentity_t *target ) {
	if ( !target || !target->client ) {
		return qtrue;
	}
	if ( target->client->pers.connected != CON_CONNECTED ) {
		return qtrue;
	}
	if ( target->client->ps.bitFlags & isDead ) {
		return qtrue;
	}
	return qfalse;
}

static gentity_t *BotLite_GetLastTrackedTargetEntity( botlite_info_t *info ) {
	if ( !info ) {
		return NULL;
	}
	if ( info->runtime.lastTargetNum < 0 || info->runtime.lastTargetNum >= level.maxclients ) {
		return NULL;
	}
	if ( !g_entities[info->runtime.lastTargetNum].inuse ) {
		return NULL;
	}
	return &g_entities[info->runtime.lastTargetNum];
}

static const char *BotLite_GoalName( botlite_goal_t goal ) {
	switch ( goal ) {
	case BOTLITE_GOAL_SEARCH: return "SEARCH";
	case BOTLITE_GOAL_COMBAT: return "COMBAT";
	case BOTLITE_GOAL_WAIT_RECOVERY: return "WAIT_RECOVERY";
	case BOTLITE_GOAL_POST_CRASH_FLYUP: return "POST_CRASH_FLYUP";
	case BOTLITE_GOAL_IDLE: return "IDLE";
	case BOTLITE_GOAL_RECOVER: return "RECOVER";
	default: return "NONE";
	}
}

static const char *BotLite_TacticName( botlite_tactic_t tactic ) {
	switch ( tactic ) {
	case BOTLITE_TACTIC_SEARCH_PATTERN: return "SEARCH_PATTERN";
	case BOTLITE_TACTIC_APPROACH: return "APPROACH";
	case BOTLITE_TACTIC_MELEE_PRESSURE: return "MELEE_PRESSURE";
	case BOTLITE_TACTIC_RANGED_PRESSURE: return "RANGED_PRESSURE";
	case BOTLITE_TACTIC_RANGED_KITE: return "RANGED_KITE";
	case BOTLITE_TACTIC_PUNISH_RECOVERY: return "PUNISH_RECOVERY";
	case BOTLITE_TACTIC_RETREAT_RECOVERY: return "RETREAT_RECOVERY";
	case BOTLITE_TACTIC_REPOSITION: return "REPOSITION";
	case BOTLITE_TACTIC_IDLE_TRACK: return "IDLE_TRACK";
	default: return "NONE";
	}
}

static void BotLite_LogGoalChange( gentity_t *bot, botlite_info_t *info, botlite_goal_t goal ) {
	if ( !bot || !info ) {
		return;
	}
	if ( info->runtime.lastGoal == goal ) {
		return;
	}
	BotLite_DebugLog( bot, va( "Goal -> %s", BotLite_GoalName( goal ) ) );
	info->runtime.lastGoal = goal;
}

static void BotLite_LogTacticChange( gentity_t *bot, botlite_info_t *info, botlite_tactic_t tactic ) {
	if ( !bot || !info ) {
		return;
	}
	if ( info->runtime.lastTactic == tactic ) {
		return;
	}
	BotLite_DebugLog( bot, va( "Tactic -> %s", BotLite_TacticName( tactic ) ) );
	info->runtime.lastTactic = tactic;
}

static void BotLite_HandleDisabledState( gentity_t *bot, int clientNum ) {
	botlite_info_t *info;
	qboolean realCrash;
	const botlite_profile_t *profile;
	int cancelDelay;

	info = &g_botlite[clientNum];
	profile = info->profile ? info->profile : BotLite_GetProfile( info->skill );
	realCrash = ( bot->client->ps.bitFlags & isCrashed ) ||
		( bot->client->ps.bitFlags & isUnconcious ) ||
		bot->client->ps.timers[tmCrash] != 0 ||
		bot->client->ps.timers[tmRecover] != 0 ||
		bot->client->ps.powerups[PW_STATE] == -1;

	if ( bot->client->ps.timers[tmKnockback] > 0 ) {
		if ( info->recovery.knockbackStartTime <= 0 || bot->client->ps.timers[tmKnockback] > 4900 ) {
			info->recovery.knockbackStartTime = level.time;
		}
		/* T2.6 -- La ventana de cancelacion exige tmKnockback < 4000 (bg_pmove.c:247):
		 * el primer segundo del knockback no se puede cancelar. Skill 2 reacciona con
		 * el doble de demora que skill 3; skill 1 no se recupera. */
		cancelDelay = profile ? profile->skill3KnockbackCancelDelay : 2000;
		if ( info->skill == 2 ) {
			cancelDelay *= 2;
		}
		if ( info->skill >= 2 && profile && ( level.time - info->recovery.knockbackStartTime ) >= cancelDelay && bot->client->ps.timers[tmKnockback] < 4000 ) {
			BotLite_EA_Button( bot, BUTTON_ALT_ATTACK );
		}
	} else {
		info->recovery.knockbackStartTime = 0;
	}

	if ( realCrash ) {
		if ( !info->recovery.postCrashFlyup ) {
			BotLite_DebugLog( bot, va( "Real crash detected on bot: dmgEv=%d tmCrash=%d tmRecover=%d", bot->client->ps.damageEvent, bot->client->ps.timers[tmCrash], bot->client->ps.timers[tmRecover] ) );
		}
		info->search.didInitialRise = qfalse;
		info->recovery.postCrashFlyup = qtrue;
	}

	BotLite_ResetTransientCombatState( clientNum );
}

static void BotLite_BuildSnapshot( gentity_t *bot, int clientNum, botlite_snapshot_t *snapshot ) {
	botlite_info_t *info;
	gentity_t *target;
	float distSq;

	info = &g_botlite[clientNum];
	memset( snapshot, 0, sizeof( *snapshot ) );
	distSq = info->profile ? info->profile->lockRange * info->profile->lockRange : ( 2200.0f * 2200.0f );
	target = NULL;

	if ( info->runtime.mode == BOTLITE_MODE_WAIT_TARGET_RECOVERY ) {
		target = BotLite_GetTrackedTarget( bot, clientNum, &distSq );
	}

	if ( info->skill == 1 && BotLite_ReactToDamage( bot, clientNum, &distSq ) ) {
		snapshot->reactedToDamage = qtrue;
		target = BotLite_GetTrackedTarget( bot, clientNum, &distSq );
	}

	if ( !target ) {
		if ( info->runtime.mode == BOTLITE_MODE_COMBAT ) {
			target = BotLite_GetTrackedTarget( bot, clientNum, &distSq );
		} else {
			/* Uses profile-configured acquisition rules: min/max distance and optional LOS.
			 * With target_acquire_max_distance=0 and target_acquire_requires_los=0 this is effectively unlimited. */
			target = BotLite_FindNearestVisiblePlayer( bot, &distSq );
			if ( target ) {
				info->runtime.lastTargetNum = target->s.number;
			}
		}
	}

	snapshot->target = target;
	snapshot->hasTarget = target ? qtrue : qfalse;
	snapshot->targetDead = BotLite_IsTargetDead( target );
	snapshot->targetStillRecovering = BotLite_TargetStillRecovering( target );
	snapshot->targetNeedsRecoveryWait = BotLite_TargetNeedsRecoveryWait( target );
	BotLite_PopulateSnapshotMetrics( bot, clientNum, target, distSq, snapshot );
	BotLite_UpdateTargetRecoveryState( bot, clientNum, target, distSq, snapshot );
}

static botlite_goal_t BotLite_SelectGoal( gentity_t *bot, int clientNum, const botlite_snapshot_t *snapshot ) {
	botlite_info_t *info;

	info = &g_botlite[clientNum];
	if ( info->recovery.postCrashFlyup ) {
		return BOTLITE_GOAL_POST_CRASH_FLYUP;
	}
	if ( info->runtime.mode == BOTLITE_MODE_WAIT_TARGET_RECOVERY ) {
		return BOTLITE_GOAL_WAIT_RECOVERY;
	}
	if ( snapshot->hasTarget && snapshot->targetCrashEdge && info->runtime.mode == BOTLITE_MODE_COMBAT ) {
		return BOTLITE_GOAL_WAIT_RECOVERY;
	}
	if ( info->runtime.mode == BOTLITE_MODE_RECOVER ) {
		return BOTLITE_GOAL_RECOVER;
	}
	if ( info->runtime.mode == BOTLITE_MODE_COMBAT ) {
		return BOTLITE_GOAL_COMBAT;
	}
	if ( info->runtime.mode == BOTLITE_MODE_SEARCH ) {
		return BOTLITE_GOAL_SEARCH;
	}
	if ( snapshot->hasTarget && ( info->skill == 2 || info->skill == 3 ) ) {
		return BOTLITE_GOAL_COMBAT;
	}
	return BOTLITE_GOAL_IDLE;
}

static void BotLite_RunSearchGoal( gentity_t *bot, int clientNum, const botlite_snapshot_t *snapshot ) {
	botlite_info_t *info;
	info = &g_botlite[clientNum];
	if ( info->skillOps && info->skillOps->RunSearch ) {
		BotLite_LogTacticChange( bot, info, BOTLITE_TACTIC_SEARCH_PATTERN );
		info->skillOps->RunSearch( bot, clientNum, snapshot );
	} else {
		BotLite_RunDefaultSearch( bot, clientNum, snapshot->target );
	}
}

static void BotLite_RunCombatGoal( gentity_t *bot, int clientNum, const botlite_snapshot_t *snapshot ) {
	botlite_info_t *info;
	gentity_t *target;
	botlite_tactic_t tactic;

	info = &g_botlite[clientNum];
	target = snapshot->target;
	if ( !target ) {
		if ( info->runtime.lastTargetNum >= 0 ) {
			BotLite_DebugLog( bot, va( "Lost target during combat lastTarget=%d", info->runtime.lastTargetNum ) );
		}
		BotLite_ClearLock( bot );
		BotLite_ResetStateForLostTarget( clientNum );
		return;
	}

	if ( snapshot->targetCrashEdge ) {
		info->runtime.targetRecoveryHandled = qtrue;
		info->runtime.targetRecoveryHandledNum = target->s.number;
		info->runtime.targetRecoveryHandledEvent = target->client->botCrashEventCounter;
		BotLite_DebugLog( bot, va( "Trigger retreat target=%d crashCounter=%d", target->s.number, target->client->botCrashEventCounter ) );
		BotLite_ClearLock( bot );
		BotLite_StartRecoveryWait( bot, clientNum, target );
		BotLite_RunRecoveryWait( bot, clientNum, target );
		return;
	}

	tactic = BOTLITE_TACTIC_IDLE_TRACK;
	if ( info->skillOps && info->skillOps->SelectCombatTactic ) {
		tactic = info->skillOps->SelectCombatTactic( bot, clientNum, snapshot );
	}
	BotLite_LogTacticChange( bot, info, tactic );
	if ( info->skillOps && info->skillOps->RunCombatTactic ) {
		info->skillOps->RunCombatTactic( bot, clientNum, snapshot, tactic );
	} else if ( target ) {
		BotLite_SetLockOn( bot, target );
		BotLite_FaceTarget( bot, target );
	}
}

static void BotLite_RunGoal( gentity_t *bot, int clientNum, botlite_goal_t goal, const botlite_snapshot_t *snapshot ) {
	switch ( goal ) {
	case BOTLITE_GOAL_POST_CRASH_FLYUP:
		BotLite_RunPostCrashFlyup( bot, clientNum );
		break;
	case BOTLITE_GOAL_WAIT_RECOVERY:
		BotLite_LogTacticChange( bot, &g_botlite[clientNum], BOTLITE_TACTIC_RETREAT_RECOVERY );
		if ( snapshot->targetCrashEdge && g_botlite[clientNum].runtime.mode != BOTLITE_MODE_WAIT_TARGET_RECOVERY && snapshot->target ) {
			BotLite_ClearLock( bot );
			BotLite_StartRecoveryWait( bot, clientNum, snapshot->target );
		}
		BotLite_RunRecoveryWait( bot, clientNum, snapshot->target );
		break;
	case BOTLITE_GOAL_SEARCH:
		BotLite_RunSearchGoal( bot, clientNum, snapshot );
		break;
	case BOTLITE_GOAL_COMBAT:
		BotLite_RunCombatGoal( bot, clientNum, snapshot );
		break;
	case BOTLITE_GOAL_RECOVER:
		BotLite_LogTacticChange( bot, &g_botlite[clientNum], BOTLITE_TACTIC_RETREAT_RECOVERY );
		BotLite_RunRecoverCycle( bot, clientNum, snapshot );
		break;
	case BOTLITE_GOAL_IDLE:
		BotLite_LogTacticChange( bot, &g_botlite[clientNum], BOTLITE_TACTIC_IDLE_TRACK );
		if ( snapshot->target ) {
			BotLite_FaceTarget( bot, snapshot->target );
		}
		break;
	default:
		break;
	}
}

void BotLite_ThinkClient( int clientNum, int time ) {
	gentity_t *bot;
	botlite_info_t *info;
	botlite_snapshot_t snapshot;
	botlite_goal_t goal;

	if ( !BotLite_IsManagedBot( clientNum ) ) {
		return;
	}

	bot = &g_entities[clientNum];
	if ( !bot->inuse || !bot->client || !( bot->r.svFlags & SVF_BOT ) ) {
		return;
	}
	if ( bot->client->pers.connected != CON_CONNECTED ) {
		return;
	}
	if ( bot->client->sess.sessionTeam == TEAM_SPECTATOR ) {
		return;
	}

	info = &g_botlite[clientNum];
	BotLite_SyncStaminaMode( bot );
	BotLite_ActionReset( clientNum, time );

	if ( bot->client->ps.bitFlags & isDead ) {
		BotLite_DebugLog( bot, "Bot died -> full respawn reset" );
		BotLite_ResetStateForRespawn( clientNum );
		BotLite_ActionCommit( bot, clientNum, time );
		return;
	}

	if ( BotLite_IsTemporarilyDisabled( bot ) ) {
		BotLite_HandleDisabledState( bot, clientNum );
		BotLite_LogGoalChange( bot, info, BOTLITE_GOAL_NONE );
		BotLite_ActionCommit( bot, clientNum, time );
		return;
	}

	if ( info->runtime.lastLoggedMode != info->runtime.mode ) {
		BotLite_DebugLog( bot, "Mode changed" );
		info->runtime.lastLoggedMode = info->runtime.mode;
	}

	BotLite_BuildSnapshot( bot, clientNum, &snapshot );
	BotLite_DebugLogSnapshot( bot, clientNum, &snapshot );

	/* El forcejeo de haces congela al bot en el sitio: se resuelve por potencia de
	 * haz, no por movimiento. Tiene prioridad sobre cualquier tactica de combate. */
	if ( BotLite_RunStruggle( bot, clientNum, &snapshot ) ) {
		BotLite_ActionCommit( bot, clientNum, time );
		return;
	}

	if ( info->skill == 3 && info->runtime.mode == BOTLITE_MODE_COMBAT && !snapshot.hasTarget && info->runtime.lastTargetNum >= 0 ) {
		gentity_t *lastTarget;
		lastTarget = BotLite_GetLastTrackedTargetEntity( info );
		if ( BotLite_IsTargetDead( lastTarget ) ) {
			BotLite_DebugLog( bot, va( "Last tracked target died -> heal window target=%d", info->runtime.lastTargetNum ) );
			BotLite_StartSkill3DeathHeal( clientNum );
			if ( info->runtime.mode == BOTLITE_MODE_WAIT_TARGET_RECOVERY ) {
				BotLite_RunRecoveryWait( bot, clientNum, NULL );
				BotLite_ActionCommit( bot, clientNum, time );
				return;
			}
		}
	}
	if ( snapshot.hasTarget && snapshot.targetDead ) {
		BotLite_DebugLog( bot, va( "Target died -> reset bot state target=%d", snapshot.target->s.number ) );
		if ( info->skill == 3 ) {
			BotLite_StartSkill3DeathHeal( clientNum );
			if ( info->runtime.mode == BOTLITE_MODE_WAIT_TARGET_RECOVERY ) {
				BotLite_RunRecoveryWait( bot, clientNum, snapshot.target );
				BotLite_ActionCommit( bot, clientNum, time );
				return;
			}
		}
		BotLite_ResetStateForTargetDeath( clientNum );
		BotLite_ActionCommit( bot, clientNum, time );
		return;
	}

	/* Fase 6.2: un zanzoken en curso se sostiene antes que nada. El motor frena
		 * el desplazamiento en seco si el boton se suelta (bg_pmove.c:317 + 320-328),
		 * asi que cualquier modulo que se lleve el frame lo aborta a mitad de camino. */
	if ( BotLite_RunZanzokenHold( bot, clientNum, &snapshot ) ) {
		BotLite_ActionCommit( bot, clientNum, time );
		return;
	}

	/* Fase 5: esquivar un proyectil entrante es la reaccion mas urgente de todas
		 * -- si impacta, el resto de las decisiones dejan de importar. Va primero.
		 * Cede el frame solo si de verdad detecto una amenaza en curso. */
	if ( BotLite_RunDodgeIncoming( bot, clientNum, &snapshot ) ) {
		BotLite_ActionCommit( bot, clientNum, time );
		return;
	}

	/* T2.5: escape con zanzoken. Se evalua ANTES que RECOVER a proposito: es una
		 * reaccion puntual a una amenaza inmediata, no depende de niveles de recurso,
		 * y antes quedaba muerto por estar acoplado al modo RECOVER. */
	if ( BotLite_RunDefensiveZanzoken( bot, clientNum, &snapshot ) ) {
		BotLite_ActionCommit( bot, clientNum, time );
		return;
	}

	/* T2.7: decidir si conviene seguir peleando. Corre antes que todo lo demas
		 * porque cambia el modo, y el resto de las decisiones dependen del modo. */
	if ( BotLite_UpdateRecoverMode( bot, clientNum, &snapshot ) ) {
		BotLite_LogGoalChange( bot, info, BOTLITE_GOAL_RECOVER );
		BotLite_RunRecoverCycle( bot, clientNum, &snapshot );
		BotLite_ActionCommit( bot, clientNum, time );
		return;
	}

	/* T3.4: empuja el ki mas alla del 100%% cuando el rival no puede castigar.
		 * Mas especifico que T2.4, va antes. */
	if ( BotLite_RunOffensiveBreakLimit( bot, clientNum, &snapshot ) ) {
		BotLite_ActionCommit( bot, clientNum, time );
		return;
	}

	/* T2.4: cargar ki es una pausa deliberada. Va antes de elegir goal porque
		 * ocupa el frame entero, y sus propias condiciones ya garantizan que solo
		 * ocurra cuando el rival no puede castigarlo. */
	if ( BotLite_RunKiCharge( bot, clientNum, &snapshot ) ) {
		BotLite_ActionCommit( bot, clientNum, time );
		return;
	}

	/* Fase 6: decidir si conviene pelear de cerca o de lejos. No consume el
	 * frame -- solo fija la intencion que despues leen la seleccion de tactica
	 * y BotLite_UpdateHybridRangeMode. */
	BotLite_UpdateEngageIntent( bot, clientNum, &snapshot );

	goal = BotLite_SelectGoal( bot, clientNum, &snapshot );
	BotLite_LogGoalChange( bot, info, goal );
	BotLite_RunGoal( bot, clientNum, goal, &snapshot );
	/* Despues de la tactica: si hay un haz en vuelo, engancha el boost para no
	 * llegar sin el a un eventual choque (ver g_botlite_struggle.c). */
	BotLite_StrugglePrepare( bot, clientNum );
	BotLite_ActionCommit( bot, clientNum, time );
}
