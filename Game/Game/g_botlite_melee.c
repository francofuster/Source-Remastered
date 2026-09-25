#include "g_local.h"
#include "g_botlite.h"

typedef enum {
	BOTLITE_MELEE_DECISION_CHASE = 0,
	BOTLITE_MELEE_DECISION_WAIT_LATCH,
	BOTLITE_MELEE_DECISION_HOLD_FREEZE,
	BOTLITE_MELEE_DECISION_START_SEQUENCE,
	BOTLITE_MELEE_DECISION_HOLD_SEQUENCE,
	BOTLITE_MELEE_DECISION_ADVANCE_SEQUENCE,
	BOTLITE_MELEE_DECISION_WAIT_SEQUENCE,
	BOTLITE_MELEE_DECISION_EXECUTE_SEQUENCE
} botlite_melee_decision_t;

typedef struct {
	float dist;
	qboolean inMelee;
	qboolean frozen;
	qboolean beyondChase;
	qboolean canPrimeLatch;
	qboolean comboIdle;
	qboolean holdingAction;
	qboolean actionExpired;
	qboolean waitingNextAction;
	const botlite_snapshot_t *snapshot;
} botlite_melee_context_t;

static void BotLite_StartCombo( int clientNum ) {
	botlite_info_t *info;
	int baseHits;
	info = &g_botlite[clientNum];
	info->melee.comboStage = BOTLITE_COMBO_PUNCH + ( rand() % 2 );
	info->melee.comboStep = 0;
	baseHits = 3 + ( rand() % 2 );
	if ( info->comboCommitment > 0.45f ) {
		baseHits++;
	}
	if ( info->comboCommitment > 0.80f ) {
		baseHits++;
	}
	if ( baseHits < 3 ) {
		baseHits = 3;
	}
	if ( baseHits > 6 ) {
		baseHits = 6;
	}
	info->melee.comboTargetHits = baseHits;
	info->melee.nextActionTime = level.time;
	info->melee.actionUntil = 0;
	BotLite_DebugLog( &g_entities[clientNum], va( "Combo started targetHits=%d", info->melee.comboTargetHits ) );
}

static void BotLite_AdvanceCombo( int clientNum ) {
	botlite_info_t *info;
	int baseHits;
	info = &g_botlite[clientNum];

	if ( info->melee.comboStage == BOTLITE_COMBO_PUNCH || info->melee.comboStage == BOTLITE_COMBO_KICK ) {
		info->melee.comboStep++;
		if ( info->melee.comboStep >= info->melee.comboTargetHits ) {
			info->melee.comboStage = BOTLITE_COMBO_SPEED;
		} else if ( ( info->melee.comboStep % 2 ) == 1 ) {
			info->melee.comboStage = BOTLITE_COMBO_SPEED;
		} else {
			info->melee.comboStage = ( info->melee.comboStage == BOTLITE_COMBO_PUNCH ) ? BOTLITE_COMBO_KICK : BOTLITE_COMBO_PUNCH;
		}
	} else if ( info->melee.comboStage == BOTLITE_COMBO_SPEED ) {
		if ( info->melee.comboStep >= info->melee.comboTargetHits ) {
			info->melee.comboStage = BOTLITE_COMBO_FINISH;
		} else {
			info->melee.comboStage = BOTLITE_COMBO_PUNCH + ( rand() % 2 );
		}
	} else {
		info->melee.comboStage = BOTLITE_COMBO_PUNCH + ( rand() % 2 );
		info->melee.comboStep = 0;
		baseHits = 3 + ( rand() % 2 );
		if ( info->comboCommitment > 0.45f ) {
			baseHits++;
		}
		if ( info->comboCommitment > 0.80f ) {
			baseHits++;
		}
		if ( baseHits > 6 ) {
			baseHits = 6;
		}
		info->melee.comboTargetHits = baseHits;
	}

	info->melee.nextActionTime = level.time + 95 + ( rand() % 85 );
	BotLite_DebugLog( &g_entities[clientNum], va( "Combo advanced stage=%d step=%d/%d", info->melee.comboStage, info->melee.comboStep, info->melee.comboTargetHits ) );
}

static void BotLite_ApplyComboDriveMovement( gentity_t *bot ) {
	BotLite_EA_MoveForward( bot, 127 );
}

static void BotLite_ApplyComboButtons( gentity_t *bot, int clientNum, botlite_info_t *info ) {
	(void)clientNum;
	BotLite_ApplyComboDriveMovement( bot );

	switch ( info->melee.comboStage ) {
	case BOTLITE_COMBO_PUNCH:
		/* Ver el guard de ExecuteMeleeSequenceInput: ATTACK produce Speed Breaker
			 * y contra un rival cargando eso le regala una carga completa. */
		BotLite_EA_Button( bot, info->melee.targetChargingNow ? BUTTON_ALT_ATTACK : BUTTON_ATTACK );
		break;
	case BOTLITE_COMBO_KICK:
		BotLite_EA_Button( bot, BUTTON_ALT_ATTACK );
		break;
	case BOTLITE_COMBO_SPEED:
		break;
	case BOTLITE_COMBO_FINISH:
		BotLite_EA_Button( bot, BUTTON_ALT_ATTACK );
		break;
	}
}

static void BotLite_ResetMeleeSequence( int clientNum ) {
	g_botlite[clientNum].melee.nextActionTime = 0;
	g_botlite[clientNum].melee.actionUntil = 0;
	g_botlite[clientNum].melee.comboTargetHits = 0;
}

static void BotLite_BeginMeleePressure( gentity_t *bot, int clientNum, gentity_t *target, const botlite_snapshot_t *snapshot ) {
	const botlite_profile_t *profile;

	BotLite_SetLockOn( bot, target );

	/* Ya enganchado en el duelo la orientacion la maneja el motor; nivelar aca
		 * solo importa mientras se esta yendo hacia el. */
	if ( snapshot && snapshot->botInMelee ) {
		BotLite_FaceTarget( bot, target );
		return;
	}
	profile = g_botlite[clientNum].profile ? g_botlite[clientNum].profile : BotLite_GetProfile( g_botlite[clientNum].skill );
	BotLite_FaceTargetLeveled( bot, target, profile ? profile->meleeApproachMaxPitch : 0.0f );
}

/*
 * Fase 6.3 -- llegar al duelo en horizontal.
 *
 * Reportado en juego: el bot suele llegar subiendo y la camara del duelo queda
 * desfasada. La causa es que PM_FlyMove arma la velocidad con los vectores de la
 * vista (bg_pmove.c:1300), asi que apuntar al rival con pitch hace que avanzar
 * sea tambien trepar.
 *
 * La solucion tiene dos mitades: la vista se nivela (BotLite_FaceTargetLeveled,
 * que deja forward horizontal y up alineado con Z) y la diferencia de altura se
 * cierra con upmove en vez de con el pitch. Si la aproximacion es muy empinada se
 * frena el avance horizontal para igualar la altura primero, y asi el ultimo tramo
 * se hace plano.
 */
/*
 * Fase 7 -- la correccion de altura era todo o nada: apenas se pasaba la
 * tolerancia, upmove saltaba a maxima potencia. Eso hacia que el bot se pasara
 * de la altura del rival, la tolerancia se cruzaba de nuevo del lado contrario
 * y upmove se invertia a maxima potencia otra vez -- un zigzag vertical continuo
 * en vez de un acercamiento lineal. Ahora la potencia escala con la distancia
 * que falta, con un piso para que la correccion se siga notando cerca del
 * limite, asi converge sin pasarse de largo.
 */
static void BotLite_ApplyApproachMovement( gentity_t *bot, int clientNum, const botlite_snapshot_t *snapshot ) {
	const botlite_profile_t *profile;
	float vertical;
	float tolerance;
	int forward;

	profile = g_botlite[clientNum].profile ? g_botlite[clientNum].profile : BotLite_GetProfile( g_botlite[clientNum].skill );
	forward = 127;

	if ( profile && snapshot && snapshot->hasTarget ) {
		vertical = snapshot->verticalDelta;
		tolerance = profile->meleeApproachLevelTolerance;
		if ( tolerance > 0.0f && ( vertical > tolerance || vertical < -tolerance ) ) {
			float magnitude;
			float rampDist;
			float scale;
			int upValue;

			magnitude = ( vertical > 0.0f ) ? vertical : -vertical;
			rampDist = tolerance * 4.0f;
			scale = ( magnitude - tolerance ) / rampDist;
			if ( scale > 1.0f ) scale = 1.0f;
			if ( scale < 0.0f ) scale = 0.0f;
			upValue = (int)( 40.0f + scale * 87.0f );
			BotLite_EA_MoveUp( bot, ( vertical > 0.0f ) ? upValue : -upValue );
			/* Mas empinado que la proporcion configurada: igualar altura primero. */
			if ( profile->meleeApproachSteepRatio > 0.0f && snapshot->horizontalDist > 1.0f ) {
				float steep;
				steep = snapshot->horizontalDist * profile->meleeApproachSteepRatio;
				if ( vertical > steep || vertical < -steep ) {
					forward = 40;
				}
			}
		}
	}

	BotLite_EA_MoveForward( bot, forward );
	BotLite_EA_BoostIfAllowed( bot, clientNum );
}

void BotLite_ApproachTargetLeveled( gentity_t *bot, int clientNum, const botlite_snapshot_t *snapshot ) {
	const botlite_profile_t *profile;

	if ( !bot || !snapshot || !snapshot->target ) {
		return;
	}
	profile = g_botlite[clientNum].profile ? g_botlite[clientNum].profile : BotLite_GetProfile( g_botlite[clientNum].skill );
	BotLite_FaceTargetLeveled( bot, snapshot->target, profile ? profile->meleeApproachMaxPitch : 0.0f );
	BotLite_ApplyApproachMovement( bot, clientNum, snapshot );
}


static void BotLite_BuildMeleeContext( gentity_t *bot, int clientNum, const botlite_snapshot_t *snapshot, botlite_melee_context_t *context ) {
	botlite_info_t *info;
	const botlite_profile_t *profile;

	info = &g_botlite[clientNum];
	profile = info->profile ? info->profile : BotLite_GetProfile( info->skill );
	memset( context, 0, sizeof( *context ) );
	context->snapshot = snapshot;
	context->dist = snapshot ? snapshot->dist : 0.0f;
	context->inMelee = snapshot ? snapshot->botInMelee : qfalse;
	context->frozen = snapshot ? snapshot->botFrozen : qfalse;
	(void)profile;
	context->beyondChase = qfalse;
	context->canPrimeLatch = ( !context->inMelee ) ? qtrue : qfalse;
	context->comboIdle = ( info->melee.nextActionTime == 0 && info->melee.actionUntil == 0 ) ? qtrue : qfalse;
	context->holdingAction = ( info->melee.actionUntil > 0 && level.time < info->melee.actionUntil ) ? qtrue : qfalse;
	context->actionExpired = ( info->melee.actionUntil > 0 && level.time >= info->melee.actionUntil ) ? qtrue : qfalse;
	context->waitingNextAction = ( level.time < info->melee.nextActionTime ) ? qtrue : qfalse;
}

static botlite_melee_decision_t BotLite_SelectMeleeDecision( const botlite_melee_context_t *context ) {
	if ( context->beyondChase ) return BOTLITE_MELEE_DECISION_CHASE;
	if ( !context->inMelee ) return BOTLITE_MELEE_DECISION_WAIT_LATCH;
	if ( context->frozen ) return BOTLITE_MELEE_DECISION_HOLD_FREEZE;
	if ( context->comboIdle ) return BOTLITE_MELEE_DECISION_START_SEQUENCE;
	if ( context->holdingAction ) return BOTLITE_MELEE_DECISION_HOLD_SEQUENCE;
	if ( context->actionExpired ) return BOTLITE_MELEE_DECISION_ADVANCE_SEQUENCE;
	if ( context->waitingNextAction ) return BOTLITE_MELEE_DECISION_WAIT_SEQUENCE;
	return BOTLITE_MELEE_DECISION_EXECUTE_SEQUENCE;
}

static void BotLite_ExecuteMeleeSequenceInput( gentity_t *bot, botlite_info_t *info ) {
	BotLite_ApplyComboDriveMovement( bot );
	if ( info->melee.comboStage == BOTLITE_COMBO_PUNCH ) {
		/* T2.2 -- Un toque de ATTACK soltado antes de 1000ms produce un Speed
			 * Breaker, y contra un rival que esta cargando eso le REGALA
			 * timers[tmMeleeCharge] = 1000, o sea una carga completa gratis
			 * (bg_pmove.c:2418-2422). Mientras el rival carga se usa ALT_ATTACK,
			 * cuyo breaker es el Charge Breaker, que juega a favor. */
		BotLite_EA_Button( bot, info->melee.targetChargingNow ? BUTTON_ALT_ATTACK : BUTTON_ATTACK );
		info->melee.actionUntil = level.time + 90;
	} else if ( info->melee.comboStage == BOTLITE_COMBO_KICK ) {
		BotLite_EA_Button( bot, BUTTON_ALT_ATTACK );
		info->melee.actionUntil = level.time + 90;
	} else if ( info->melee.comboStage == BOTLITE_COMBO_SPEED ) {
		info->melee.actionUntil = level.time + 240;
	} else {
		/* T2.1 -- El remate era un Power melee encubierto: manteniendo ALT_ATTACK
			 * 650ms superaba el umbral de 550ms sin que nadie lo decidiera. Ahora es
			 * un golpe corto y el Power sale unicamente de BotLite_RunDeliberateCharge,
			 * que si mira el estado del rival. Consecuencia buscada: skill 1, que no
			 * tiene allowsDeliberateCharge, deja de conectar Power melee. */
		BotLite_EA_Button( bot, BUTTON_ALT_ATTACK );
		info->melee.actionUntil = level.time + 90;
	}
}

static void BotLite_ExecuteMeleeDecision( gentity_t *bot, int clientNum, botlite_info_t *info, const botlite_melee_context_t *context, botlite_melee_decision_t decision ) {
	if ( decision == BOTLITE_MELEE_DECISION_CHASE ) {
		BotLite_ResetMeleeSequence( clientNum );
		BotLite_ApplyApproachMovement( bot, clientNum, context->snapshot );
		return;
	}
	if ( decision == BOTLITE_MELEE_DECISION_WAIT_LATCH ) {
		if ( context->canPrimeLatch ) {
			BotLite_ResetMeleeSequence( clientNum );
		}
		BotLite_ApplyApproachMovement( bot, clientNum, context->snapshot );
		return;
	}
	if ( decision == BOTLITE_MELEE_DECISION_HOLD_FREEZE ) {
		return;
	}
	if ( decision == BOTLITE_MELEE_DECISION_START_SEQUENCE ) {
		BotLite_StartCombo( clientNum );
		BotLite_ExecuteMeleeSequenceInput( bot, info );
		return;
	}
	if ( decision == BOTLITE_MELEE_DECISION_HOLD_SEQUENCE ) {
		BotLite_ApplyComboButtons( bot, clientNum, info );
		return;
	}
	if ( decision == BOTLITE_MELEE_DECISION_ADVANCE_SEQUENCE ) {
		BotLite_ApplyComboDriveMovement( bot );
		info->melee.actionUntil = 0;
		BotLite_AdvanceCombo( clientNum );
		return;
	}
	if ( decision == BOTLITE_MELEE_DECISION_WAIT_SEQUENCE ) {
		BotLite_ApplyComboDriveMovement( bot );
		return;
	}
	BotLite_ExecuteMeleeSequenceInput( bot, info );
}

void BotLite_RunMeleePressure( gentity_t *bot, int clientNum, gentity_t *target, const botlite_snapshot_t *snapshot ) {
	botlite_info_t *info;
	botlite_melee_context_t context;
	botlite_melee_decision_t decision;

	if ( !bot || !target ) {
		return;
	}

	info = &g_botlite[clientNum];
	BotLite_BeginMeleePressure( bot, clientNum, target, snapshot );
	BotLite_BuildMeleeContext( bot, clientNum, snapshot, &context );
	decision = BotLite_SelectMeleeDecision( &context );
	BotLite_ExecuteMeleeDecision( bot, clientNum, info, &context, decision );
}

static const char *BotLite_CombatStateName( int state ) {
	switch ( state ) {
	case BOTLITE_COMBAT_STATE_APPROACH: return "APPROACH";
	case BOTLITE_COMBAT_STATE_SANZOKEN_APPROACH: return "SANZOKEN_APPROACH";
	case BOTLITE_COMBAT_STATE_RANGED_PRESSURE: return "RANGED_PRESSURE";
	case BOTLITE_COMBAT_STATE_MELEE_COMBO: return "MELEE_COMBO";
	case BOTLITE_COMBAT_STATE_BLOCK: return "BLOCK";
	case BOTLITE_COMBAT_STATE_SPECIAL: return "SPECIAL";
	case BOTLITE_COMBAT_STATE_FROZEN: return "FROZEN";
	default: return "NONE";
	}
}

static void BotLite_CombatClearTransientDodgeState( botlite_info_t *info ) {
	if ( !info ) {
		return;
	}
	info->melee.dodgeUntil = 0;
	info->melee.dodgeDirection = 1;
}

static void BotLite_CombatSetState( gentity_t *bot, botlite_info_t *info, int state, int thinkMs ) {
	if ( !info ) {
		return;
	}
	if ( info->melee.combatState != state ) {
		info->melee.combatState = state;
		info->melee.combatStateChangedTime = level.time;
		if ( bot ) {
			BotLite_DebugLog( bot, va( "Combat state -> %s", BotLite_CombatStateName( state ) ) );
		}
	}
	info->melee.combatNextThinkTime = level.time + thinkMs;
}

static int BotLite_GetSanzokenInterval( const botlite_profile_t *profile ) {
	int interval;

	interval = profile ? profile->skill3DodgeMinInterval : 3000;
	if ( interval <= 0 ) {
		interval = 3000;
	}
	return interval;
}

static qboolean BotLite_CanApproachWithSanzoken( int clientNum, const botlite_melee_context_t *context, const botlite_snapshot_t *snapshot ) {
	botlite_info_t *info;

	if ( clientNum < 0 || clientNum >= level.maxclients || !context || !snapshot ) {
		return qfalse;
	}

	if ( context->inMelee || context->frozen ) {
		return qfalse;
	}

	info = &g_botlite[clientNum];
	if ( info->melee.blockUntil > level.time || info->melee.specialHoldUntil > level.time ) {
		return qfalse;
	}

	/* Keep sanzoken available during approach even if the target is briefly out of LOS
	 * or the bot is still clearing short action timers from a previous ranged step.
	 * The engine will still reject teleport when the underlying state truly forbids it. */
	return snapshot->hasTarget ? qtrue : qfalse;
}

static void BotLite_UseApproachSanzoken( gentity_t *bot, botlite_info_t *info, const botlite_profile_t *profile ) {
	if ( !bot || !info ) {
		return;
	}

	info->melee.dodgeUntil = 0;
	info->melee.dodgeDirection = 1;
	info->melee.nextDodgeTime = level.time + BotLite_GetSanzokenInterval( profile );
	BotLite_DebugLog( bot, va( "Use approach sanzoken next=%d", info->melee.nextDodgeTime ) );
	/* Sostenido: un toque suelto pagaba la fatiga entera sin avanzar nada.
	 * Adelante, que es hacia donde ya esta mirando al rival. */
	BotLite_StartZanzoken( bot, bot->s.number, 127, 0, 0 );
}

static int BotLite_RandomCombatInterval( int minValue, int maxValue ) {
	if ( maxValue <= minValue ) {
		return minValue;
	}
	return minValue + ( rand() % ( maxValue - minValue + 1 ) );
}

static void BotLite_ApplyKnockbackDirection( gentity_t *bot, int direction ) {
	if ( direction == BOTLITE_SKILL3_KB_UP ) {
		BotLite_EA_MoveUp( bot, 127 );
	} else if ( direction == BOTLITE_SKILL3_KB_DOWN ) {
		BotLite_EA_MoveUp( bot, -127 );
	}
}

static void BotLite_PrimeSpecialAttack( botlite_info_t *info, const botlite_profile_t *profile ) {
	int directionRoll;

	if ( !info || !profile ) {
		return;
	}

	directionRoll = rand() % 3;
	if ( directionRoll == 0 ) {
		info->melee.knockbackDirection = BOTLITE_SKILL3_KB_UP;
	} else if ( directionRoll == 1 ) {
		info->melee.knockbackDirection = BOTLITE_SKILL3_KB_DOWN;
	} else {
		info->melee.knockbackDirection = BOTLITE_SKILL3_KB_FORWARD;
	}
	info->melee.specialHoldUntil = level.time + profile->skill3PowerMeleeChargeTime;
}

static int BotLite_PickManagedCombatState( gentity_t *bot, int clientNum, botlite_info_t *info, const botlite_profile_t *profile, const botlite_combat_policy_t *policy, const botlite_melee_context_t *context, const botlite_snapshot_t *snapshot ) {
	int duration;
	int interval;
	int blockChance;
	int specialChance;

	if ( !bot || !info || !profile || !policy || !context || !snapshot ) {
		return BOTLITE_COMBAT_STATE_NONE;
	}

	if ( context->frozen ) {
		return BOTLITE_COMBAT_STATE_FROZEN;
	}

	if ( policy->allowsBlock ) {
		if ( info->melee.blockUntil > level.time ) {
			return context->inMelee ? BOTLITE_COMBAT_STATE_BLOCK : BOTLITE_COMBAT_STATE_APPROACH;
		}
		if ( info->melee.blockUntil != 0 && info->melee.blockUntil <= level.time ) {
			info->melee.blockUntil = 0;
		}
	}

	if ( policy->allowsSpecial ) {
		if ( info->melee.specialHoldUntil > level.time ) {
			return BOTLITE_COMBAT_STATE_SPECIAL;
		}
		if ( info->melee.specialHoldUntil != 0 ) {
			info->melee.specialHoldUntil = 0;
			info->melee.nextActionTime = level.time + 180 + ( rand() % 140 );
		}
	}

	if ( policy->allowsApproachSanzoken && info->melee.nextDodgeTime <= 0 ) {
		/* Initialize as ready-now: the cfg interval is a cooldown between uses,
		 * not an extra mandatory startup delay before the first teleport. */
		info->melee.nextDodgeTime = level.time;
	}
	if ( policy->allowsBlock && info->melee.nextBlockTime <= 0 ) {
		info->melee.nextBlockTime = level.time + BotLite_RandomCombatInterval( profile->skill3BlockMinInterval, profile->skill3BlockMaxInterval );
	}

	if ( !context->inMelee ) {
		if ( policy->allowsApproachSanzoken &&
			 level.time >= info->melee.nextDodgeTime &&
			 BotLite_CanApproachWithSanzoken( clientNum, context, snapshot ) &&
			 BotLite_ShouldUseSanzoken( bot, clientNum ) ) {
			return BOTLITE_COMBAT_STATE_SANZOKEN_APPROACH;
		}
		return BOTLITE_COMBAT_STATE_APPROACH;
	}

	if ( policy->allowsBlock && level.time >= info->melee.nextBlockTime ) {
		blockChance = 22 + (int)( info->blockTendency * 58.0f );
		if ( snapshot->targetCharging ||
			 ( snapshot->targetInMelee && ( rand() % 100 ) < blockChance + 18 ) ||
			 ( rand() % 100 ) < blockChance ) {
			duration = BotLite_RandomCombatInterval( profile->skill3BlockMinDuration, profile->skill3BlockMaxDuration );
			interval = BotLite_RandomCombatInterval( profile->skill3BlockMinInterval, profile->skill3BlockMaxInterval );
			info->melee.blockUntil = level.time + duration;
			info->melee.nextBlockTime = level.time + interval;
			return BOTLITE_COMBAT_STATE_BLOCK;
		}
		info->melee.nextBlockTime = level.time + 250;
	}

	if ( policy->allowsSpecial && !context->holdingAction && !context->waitingNextAction && level.time >= info->melee.nextActionTime ) {
		specialChance = 10 + (int)( info->specialTendency * 55.0f );
		if ( ( rand() % 100 ) < specialChance ) {
			BotLite_PrimeSpecialAttack( info, profile );
			return BOTLITE_COMBAT_STATE_SPECIAL;
		}
	}

	return BOTLITE_COMBAT_STATE_MELEE_COMBO;
}

static qboolean BotLite_ShouldPulseApproachSanzoken( gentity_t *bot, int clientNum, const botlite_snapshot_t *snapshot, botlite_info_t *info, const botlite_combat_policy_t *policy, const botlite_melee_context_t *context ) {
	if ( !bot || !snapshot || !info || !policy || !context ) {
		return qfalse;
	}
	if ( !policy->allowsApproachSanzoken ) {
		return qfalse;
	}
	if ( level.time < info->melee.nextDodgeTime ) {
		return qfalse;
	}
	if ( !BotLite_CanApproachWithSanzoken( clientNum, context, snapshot ) ) {
		return qfalse;
	}
	if ( !BotLite_ShouldUseSanzoken( bot, clientNum ) ) {
		return qfalse;
	}
	return qtrue;
}

static void BotLite_RunManagedCombatState( gentity_t *bot, int clientNum, gentity_t *target, const botlite_snapshot_t *snapshot, botlite_info_t *info, const botlite_combat_policy_t *policy ) {
	if ( !bot || !info || !policy ) {
		return;
	}

	switch ( info->melee.combatState ) {
	case BOTLITE_COMBAT_STATE_APPROACH:
		if ( snapshot && snapshot->botInMelee ) {
			BotLite_CombatSetState( bot, info, BOTLITE_COMBAT_STATE_MELEE_COMBO, policy->tacticalThinkMs );
			BotLite_RunMeleePressure( bot, clientNum, target, snapshot );
			break;
		}
		if ( snapshot ) {
			botlite_melee_context_t pulseContext;
			BotLite_BuildMeleeContext( bot, clientNum, snapshot, &pulseContext );
			if ( BotLite_ShouldPulseApproachSanzoken( bot, clientNum, snapshot, info, policy, &pulseContext ) ) {
				BotLite_UseApproachSanzoken( bot, info, info->profile ? info->profile : BotLite_GetProfile( info->skill ) );
				break;
			}
		}
		BotLite_ApplyApproachMovement( bot, clientNum, snapshot );
		break;
	case BOTLITE_COMBAT_STATE_SANZOKEN_APPROACH:
		BotLite_UseApproachSanzoken( bot, info, info->profile ? info->profile : BotLite_GetProfile( info->skill ) );
		BotLite_CombatSetState( bot, info, BOTLITE_COMBAT_STATE_APPROACH, policy->tacticalThinkMs );
		break;
	case BOTLITE_COMBAT_STATE_BLOCK:
		if ( !snapshot || !snapshot->botInMelee || info->melee.blockUntil <= level.time ) {
			BotLite_CombatSetState( bot, info, BOTLITE_COMBAT_STATE_APPROACH, policy->tacticalThinkMs );
			break;
		}
		BotLite_EA_Button( bot, BUTTON_BLOCK );
		break;
	case BOTLITE_COMBAT_STATE_SPECIAL:
		if ( info->melee.specialHoldUntil <= level.time ) {
			BotLite_CombatSetState( bot, info, BOTLITE_COMBAT_STATE_MELEE_COMBO, policy->tacticalThinkMs );
			break;
		}
		BotLite_EA_Button( bot, BUTTON_ALT_ATTACK );
		BotLite_ApplyKnockbackDirection( bot, info->melee.knockbackDirection );
		break;
	case BOTLITE_COMBAT_STATE_FROZEN:
		break;
	case BOTLITE_COMBAT_STATE_RANGED_PRESSURE:
		BotLite_RunRangedPressure( bot, clientNum, target, snapshot );
		break;
	case BOTLITE_COMBAT_STATE_MELEE_COMBO:
	default:
		BotLite_RunMeleePressure( bot, clientNum, target, snapshot );
		break;
	}
}

/*
 * T1.1 -- Bloqueo reactivo.
 *
 * El bloqueo por temporizador aleatorio (BotLite_PickManagedCombatState) no lee al
 * rival: bloquea en momentos arbitrarios y deja pasar las cargas reales. Esto lo
 * reemplaza por una respuesta a stMeleeState del oponente, que es el dato que
 * resuelve el duelo de melee (bg_pmove.c:2358 y siguientes).
 *
 * Bloquear un Power conectado reduce el dano al 30% (bg_pmove.c:2447), asi que la
 * ventana que importa es mientras el rival carga, no despues.
 */
qboolean BotLite_TargetIsChargingMelee( const botlite_snapshot_t *snapshot ) {
	if ( !snapshot || !snapshot->hasTarget ) {
		return qfalse;
	}

	switch ( snapshot->targetMeleeState ) {
	case stMeleeChargingPower:
	case stMeleeChargingStun:
	case stMeleeStartPower:
		return qtrue;
	default:
		return qfalse;
	}
}

/*
 * T3.2 -- Lectura de patron del rival.
 *
 * Charge Breaker es el UNICO contra real contra una carga (Power o Stun, sin
 * distincion -- corregido en T2.2), asi que no hay 'que breaker uso' que leer.
 * Lo que si vale la pena rastrear es otra asimetria de T3.1: Speed Breaker
 * atraviesa el Evade pero lo detiene el Block (bg_pmove.c:2417, sin gate de
 * Evade). Un rival que abusa de Speed Breaker esta explotando exactamente la
 * debilidad de Evade -- si el bot lo nota, conviene sesgar hacia Block.
 *
 * EMA simple en vez de un buffer circular explicito: cada vez que se ve al rival
 * ENTRAR en stMeleeUsingSpeedBreaker (una vez por ocurrencia, no por frame), el
 * sesgo sube; decae de a poco en cada llamada mientras siguen los intercambios.
 * Persiste entre exchanges (vive en runtime, no en melee -- que se limpia en
 * cada transicion), y se reinicia solo al cambiar de objetivo.
 */
static void BotLite_UpdateSpeedBreakerPattern( int clientNum, const botlite_snapshot_t *snapshot ) {
	botlite_info_t *info;
	int targetNum;

	if ( !snapshot || !snapshot->hasTarget || !snapshot->target ) {
		return;
	}

	info = &g_botlite[clientNum];
	targetNum = snapshot->target->s.number;
	if ( info->runtime.patternTargetNum != targetNum ) {
		info->runtime.patternTargetNum = targetNum;
		/* Prior neutro: sin datos todavia, pero no 100%% Evade desde el arranque
		 * -- eso seria tan leible como cualquier otro patron fijo. */

		info->runtime.targetSpeedBreakerBias = 0.3f;
		info->runtime.speedBreakerEdgeSeen = qfalse;
	}

	if ( snapshot->targetMeleeState == stMeleeUsingSpeedBreaker ) {
		if ( !info->runtime.speedBreakerEdgeSeen ) {
			info->runtime.speedBreakerEdgeSeen = qtrue;
			info->runtime.targetSpeedBreakerBias = info->runtime.targetSpeedBreakerBias * 0.7f + 0.3f;
		}
	} else {
		info->runtime.speedBreakerEdgeSeen = qfalse;
		info->runtime.targetSpeedBreakerBias *= 0.999f;
	}
}
static void BotLite_ResetReactiveBlock( botlite_info_t *info ) {
	info->melee.reactiveChargeSeen = qfalse;
	info->melee.reactiveBlockCommitted = qfalse;
	info->melee.reactiveBlockReadyTime = 0;
	info->melee.reactiveBlockUntil = 0;
	info->melee.reactiveDefenseIsEvade = qfalse;
}

/*
 * T3.1 -- Evade como opcion real, mezclada con Block.
 *
 * Tabla de contras verificada linea por linea contra bg_pmove.c (no asumida):
 *
 *              | Speed hit         | Power hit           | Charge Breaker | Speed Breaker
 *   Block      | reduce a 20% dmg  | reduce a 30% dmg     | NO lo detiene  | SI lo detiene
 *   Evade      | anula, ataca paga | anula, evade paga    | anula GRATIS   | NO lo detiene
 *              | fatiga extra      | 0.4x fatiga (barato) | (:2404)        | (:2417, sin gate)
 *
 * O sea Evade es estrictamente mejor que Block contra Speed/Power/Charge-Breaker,
 * pero un rival que lo note puede castigarlo con Speed Breaker, que Evade no para
 * y Block si. Por eso NO conviene Evadir siempre aunque sea 'mejor' en el momento:
 * eso lo volveria a el mismo predecible y explotable. Se mezcla con probabilidad.
 *
 * Ademas Evade no usa boton: es forwardmove<0 mientras el bot sigue encerrado en
 * melee (bg_pmove.c:2606-2609). Por eso, a diferencia de Block, no compite con
 * SanitizeButtons por otros botones.
 */

/* Devuelve qtrue si tomo el control del frame para sostener Block o Evade. */
static qboolean BotLite_RunReactiveBlock( gentity_t *bot, int clientNum, const botlite_snapshot_t *snapshot,
								  const botlite_profile_t *profile, const botlite_combat_policy_t *policy ) {
	botlite_info_t *info;
	qboolean charging;
	int chance;

	if ( !bot || !policy || !profile || !snapshot ) {
		return qfalse;
	}

	info = &g_botlite[clientNum];

	if ( !policy->allowsReactiveBlock || !snapshot->botInMelee ) {
		BotLite_ResetReactiveBlock( info );
		return qfalse;
	}

	charging = BotLite_TargetIsChargingMelee( snapshot );

	/* Una sola tirada por carga del rival. Evaluarla cada frame haria que el bot
		 * reaccionara practicamente siempre, que es tan artificial como el timer. */
	if ( charging && !info->melee.reactiveChargeSeen ) {
		info->melee.reactiveChargeSeen = qtrue;
		chance = (int)( info->blockTendency * 100.0f );
		info->melee.reactiveBlockCommitted = ( ( rand() % 100 ) < chance ) ? qtrue : qfalse;
		/* Con Evade habilitado, la mitad de las veces que se compromete a defender
			 * usa Evade en vez de Block. Es la variacion que evita que el bot mismo se
			 * vuelva leible con Speed Breaker. */
		{
			/*
			 * Fase 6.6: el Evade quedo restringido al estado critico.
			 *
			 * evadeChance arrancaba en 100 con bias 0, o sea que por defecto el bot
			 * SIEMPRE esquivaba en vez de bloquear, y solo bajaba si el rival abusaba
			 * del Speed Breaker. Reportado en juego como esquivar en vez de pelear.
			 *
			 * Bloquear mantiene al bot dentro del intercambio; esquivar lo saca. Fuera
			 * de una emergencia, quedarse a pelear es lo que corresponde.
			 */
			int evadeChance = 100 - (int)( info->runtime.targetSpeedBreakerBias * 70.0f );
			if ( evadeChance < 10 ) { evadeChance = 10; }
			if ( evadeChance > 90 ) { evadeChance = 90; }
			info->melee.reactiveDefenseIsEvade = ( policy->allowsEvade && info->melee.reactiveBlockCommitted &&
				BotLite_InCriticalState( bot, clientNum ) &&
				( rand() % 100 ) < evadeChance ) ? qtrue : qfalse;
		}
		info->melee.reactiveBlockReadyTime = level.time +
			BotLite_RandomCombatInterval( profile->reactiveBlockMinReaction, profile->reactiveBlockMaxReaction );
		info->melee.reactiveBlockUntil = 0;
		BotLite_DebugLog( bot, va( "Reactive defense: target=%s chance=%d%% bias=%d%% -> %s",
			BotLite_MeleeStateName( snapshot->targetMeleeState ),
			chance,
			(int)( info->runtime.targetSpeedBreakerBias * 100.0f ),
			!info->melee.reactiveBlockCommitted ? "ignore" :
				( info->melee.reactiveDefenseIsEvade ? "EVADE" : "BLOCK" ) ) );
	} else if ( !charging && info->melee.reactiveChargeSeen && info->melee.reactiveBlockUntil == 0 ) {
		/* La carga termino antes de que llegara la reaccion: se perdio la ventana. */
		BotLite_ResetReactiveBlock( info );
	}

	if ( !info->melee.reactiveBlockCommitted ) {
		return qfalse;
	}
	if ( level.time < info->melee.reactiveBlockReadyTime ) {
		return qfalse;
	}

	if ( info->melee.reactiveBlockUntil == 0 ) {
		info->melee.reactiveBlockUntil = level.time +
			BotLite_RandomCombatInterval( profile->skill3BlockMinDuration, profile->skill3BlockMaxDuration );
	}

	if ( level.time >= info->melee.reactiveBlockUntil ) {
		BotLite_ResetReactiveBlock( info );
		return qfalse;
	}

	if ( info->melee.reactiveDefenseIsEvade ) {
		BotLite_EA_MoveForward( bot, -127 );
	} else {
		BotLite_EA_Button( bot, BUTTON_BLOCK );
	}
	return qtrue;
}

/*
 * T2.1 -- Cargas de Power/Stun deliberadas.
 *
 * Hoy el bot conecta Power melee por accidente: el remate del combo mantiene
 * ALT_ATTACK 650ms y eso supera el umbral de 550ms. Esto lo convierte en una
 * decision que mira el estado del rival.
 *
 * Ademas hay dos rutas a un Power melee INSTANTANEO al entrar en melee, que no
 * requieren cargar nada (bg_pmove.c:2547-2562). Ambas otorgan meleeCharge=750 de
 * golpe al iniciar la secuencia con forwardmove>0 y sin botones de ataque:
 *
 *   A) el rival esta en knockback (timers[tmKnockback] != 0)
 *   B) el bot viene boosteando sostenido mas de 2500ms (timers[tmBoost] > 2500),
 *      que ademas deja al rival en stMeleeStartHit
 *
 * La ruta B se rompe sola si el bot presiona ataque mientras se acerca:
 * BotLite_SanitizeButtons descarta BOOST cuando hay ATTACK/ALT_ATTACK, PM_CheckBoost
 * llama a PM_StopBoost y tmBoost vuelve a 0.
 */

#define BOTLITE_CHARGE_NONE	0
#define BOTLITE_CHARGE_POWER	1
#define BOTLITE_CHARGE_STUN		2

static qboolean BotLite_TargetIsVulnerableToCharge( const botlite_snapshot_t *snapshot ) {
	if ( snapshot->targetKnockbackTime > 0 ) {
		return qtrue;
	}
	switch ( snapshot->targetMeleeState ) {
	case stMeleeStartHit:
	case stMeleeIdle:
	case stMeleeStartDodge:
		return qtrue;
	default:
		return qfalse;
	}
}

static int BotLite_SelectDeliberateCharge( int clientNum, const botlite_snapshot_t *snapshot ) {
	botlite_info_t *info;
	int chance;

	info = &g_botlite[clientNum];

	/* Si el rival esta cargando, comprometerse a una carga larga es perder el
		 * intercambio: su ataque sale antes. Esa situacion es del Breaker (T2.2). */
	if ( BotLite_TargetIsChargingMelee( snapshot ) ) {
		return BOTLITE_CHARGE_NONE;
	}

	/* Evade anula Power y Stun por igual (bg_pmove.c:2445): no gastar la ventana. */
	if ( snapshot->targetMeleeState == stMeleeUsingEvade ) {
		return BOTLITE_CHARGE_NONE;
	}

	if ( BotLite_TargetIsVulnerableToCharge( snapshot ) ) {
		chance = 45 + (int)( info->aggression * 45.0f );
	} else {
		chance = (int)( info->comboCommitment * 25.0f );
	}

	if ( ( rand() % 100 ) >= chance ) {
		return BOTLITE_CHARGE_NONE;
	}

	/* Contra un rival bloqueando, el Power pasa al 30% de dano (bg_pmove.c:2447).
		 * El Stun no depende de eso, asi que es la opcion util ahi. */
	if ( snapshot->targetMeleeState == stMeleeUsingBlock ) {
		return BOTLITE_CHARGE_STUN;
	}
	return BOTLITE_CHARGE_POWER;
}

qboolean BotLite_RunDeliberateCharge( gentity_t *bot, int clientNum, const botlite_snapshot_t *snapshot ) {
	botlite_info_t *info;
	const botlite_profile_t *profile;
	const botlite_combat_policy_t *policy;
	int choice;
	int hold;

	if ( !bot || !snapshot ) {
		return qfalse;
	}

	info = &g_botlite[clientNum];
	profile = info->profile ? info->profile : BotLite_GetProfile( info->skill );
	policy = BotLite_GetCombatPolicy( info->skill );

	if ( !policy || !policy->allowsDeliberateCharge ) {
		return qfalse;
	}

	/* Sostener una carga en curso. Se aborta si el rival sale del melee: mantener
		 * el boton sin nadie enfrente solo congela al bot. */
	if ( info->melee.chargeCommitUntil > level.time ) {
		if ( !snapshot->botInMelee ) {
			info->melee.chargeCommitUntil = 0;
			BotLite_DebugLog( bot, "Charge aborted: left melee" );
			return qfalse;
		}
		BotLite_ApplyComboDriveMovement( bot );
		BotLite_EA_Button( bot, info->melee.chargeCommitButton );
		return qtrue;
	}

	if ( info->melee.chargeCommitUntil != 0 ) {
		info->melee.chargeCommitUntil = 0;
		info->melee.chargeNextCommitTime = level.time + profile->chargeCommitCooldown;
	}

	if ( !snapshot->botInMelee || level.time < info->melee.chargeNextCommitTime ) {
		return qfalse;
	}

	/* Power cuesta plMaximum*0.05 de fatiga (bg_pmove.c:2437). */
	if ( !BotLite_StaminaAllowsSpend( clientNum, BOTLITE_SPEND_NORMAL ) ) {
		return qfalse;
	}

	choice = BotLite_SelectDeliberateCharge( clientNum, snapshot );
	if ( choice == BOTLITE_CHARGE_NONE ) {
		return qfalse;
	}

	if ( choice == BOTLITE_CHARGE_STUN ) {
		info->melee.chargeCommitButton = BUTTON_ATTACK;
		hold = profile->stunChargeHoldMs;
	} else {
		info->melee.chargeCommitButton = BUTTON_ALT_ATTACK;
		hold = profile->skill3PowerMeleeChargeTime;
	}
	if ( hold < 100 ) {
		hold = 100;
	}

	info->melee.chargeCommitUntil = level.time + hold;
	BotLite_DebugLog( bot, va( "Charge commit: %s vs %s hold=%dms",
		( choice == BOTLITE_CHARGE_STUN ) ? "STUN" : "POWER",
		BotLite_MeleeStateName( snapshot->targetMeleeState ),
		hold ) );

	BotLite_ApplyComboDriveMovement( bot );
	BotLite_EA_Button( bot, info->melee.chargeCommitButton );
	return qtrue;
}

/*
 * T2.2 -- Charge Breaker reactivo.
 *
 * Tabla real de contras, verificada en bg_pmove.c:2386-2428. El backlog la tenia
 * INVERTIDA, y el error importa mucho:
 *
 *   Charge Breaker (tmMeleeBreaker = +1, se produce tocando ALT_ATTACK y soltando
 *   antes de los 550ms):
 *     - contra un rival CARGANDO: exito. Le pone timers[tmMeleeCharge] = 0, o sea
 *       le rompe la carga, le hace dano y lo congela 500ms. Ademas suma healthPool
 *       y maximumPool al que lo ejecuta.
 *     - contra stMeleeUsingSpeed: BACKFIRE. El bot queda congelado 950ms.
 *     - contra Evade: sin efecto.
 *
 *   Speed Breaker (tmMeleeBreaker = -1, tocando ATTACK y soltando antes de 1000ms):
 *     - contra un rival CARGANDO: BACKFIRE CATASTROFICO. Le regala
 *       timers[tmMeleeCharge] = 1000, es decir una carga COMPLETA gratis.
 *
 * Consecuencia para el combo: el stage PUNCH toca ATTACK 90ms y lo suelta, o sea
 * produce un Speed Breaker. Si el rival esta cargando en ese momento, el bot le
 * estaba regalando una carga completa. Por eso ademas del breaker deliberado hay
 * un guard que prohibe los toques de ATTACK mientras el rival carga.
 */

/* Toque corto: hay que soltar antes de 550ms para que salga breaker y no Power. */
/* T4.1: duracion del toque -- ahora profile->breakerTapMs. */

static qboolean BotLite_BotIsInSpeedMelee( const botlite_snapshot_t *snapshot ) {
	return ( snapshot->botMeleeState == stMeleeUsingSpeed ) ? qtrue : qfalse;
}

qboolean BotLite_RunReactiveBreaker( gentity_t *bot, int clientNum, const botlite_snapshot_t *snapshot ) {
	botlite_info_t *info;
	const botlite_combat_policy_t *policy;
	const botlite_profile_t *profile;
	qboolean charging;

	if ( !bot || !snapshot ) {
		return qfalse;
	}

	info = &g_botlite[clientNum];
	policy = BotLite_GetCombatPolicy( info->skill );
	profile = info->profile ? info->profile : BotLite_GetProfile( info->skill );

	if ( !policy || !policy->allowsBreaker || !snapshot->botInMelee ) {
		info->melee.breakerHoldUntil = 0;
		info->melee.breakerEpisodeUsed = qfalse;
		return qfalse;
	}

	/* Sostener el toque. Al vencer, NO se presiona mas: esa liberacion es lo que
		 * dispara el breaker (bg_pmove.c:2470). */
	if ( info->melee.breakerHoldUntil > level.time ) {
		BotLite_ApplyComboDriveMovement( bot );
		BotLite_EA_Button( bot, BUTTON_ALT_ATTACK );
		return qtrue;
	}
	if ( info->melee.breakerHoldUntil != 0 ) {
		info->melee.breakerHoldUntil = 0;
		/* Frame de liberacion: mover sin atacar para que el motor procese el breaker. */
		BotLite_ApplyComboDriveMovement( bot );
		return qtrue;
	}

	charging = BotLite_TargetIsChargingMelee( snapshot );
	if ( !charging ) {
		info->melee.breakerEpisodeUsed = qfalse;
		return qfalse;
	}

	/* Un intento por carga del rival. */
	if ( info->melee.breakerEpisodeUsed || level.time < info->melee.breakerNextTime ) {
		return qfalse;
	}

	/* Con el bot en Speed el Charge Breaker rebota y lo congela 950ms. */
	if ( BotLite_BotIsInSpeedMelee( snapshot ) ) {
		return qfalse;
	}

	/* Evade del rival anula el breaker: no gastar el cooldown. */
	if ( snapshot->targetMeleeState == stMeleeUsingEvade ) {
		return qfalse;
	}

	info->melee.breakerEpisodeUsed = qtrue;
	info->melee.breakerHoldUntil = level.time + profile->breakerTapMs;
	/* tmMeleeBreakerWait impone 500ms entre breakers (bg_pmove.c:2472). */
	info->melee.breakerNextTime = level.time + 650;
	BotLite_DebugLog( bot, va( "Charge breaker vs %s",
		BotLite_MeleeStateName( snapshot->targetMeleeState ) ) );

	BotLite_ApplyComboDriveMovement( bot );
	BotLite_EA_Button( bot, BUTTON_ALT_ATTACK );
	return qtrue;
}

void BotLite_RunManagedMeleePressure( gentity_t *bot, int clientNum, gentity_t *target, const botlite_snapshot_t *snapshot ) {
	botlite_info_t *info;
	botlite_melee_context_t context;
	const botlite_profile_t *profile;
	const botlite_combat_policy_t *policy;
	int nextState;

	if ( !bot || !target ) {
		return;
	}

	info = &g_botlite[clientNum];
	profile = info->profile ? info->profile : BotLite_GetProfile( info->skill );
	policy = BotLite_GetCombatPolicy( info->skill );
	BotLite_BeginMeleePressure( bot, clientNum, target, snapshot );
	BotLite_BuildMeleeContext( bot, clientNum, snapshot, &context );
	BotLite_CombatClearTransientDodgeState( info );
	info->melee.targetChargingNow = BotLite_TargetIsChargingMelee( snapshot );
	BotLite_UpdateSpeedBreakerPattern( clientNum, snapshot );

	/* El bloqueo reactivo es una respuesta a la carga del rival, no una tactica
		 * elegida: se resuelve antes que la maquina de estados de combate. */
	/* El Charge Breaker es estrictamente mejor que bloquear cuando esta
		 * disponible: rompe la carga en vez de amortiguarla, y encima da recursos.
		 * Por eso va antes. El bloqueo queda como respaldo para skill 1. */
	if ( BotLite_RunReactiveBreaker( bot, clientNum, snapshot ) ) {
		return;
	}

	/* T2.5 -- El zanzoken NO se puede usar desde aca: PM_CheckZanzoken se niega
		 * por completo mientras usingMelee este puesto (bg_pmove.c:316), y ese flag
		 * queda fijo durante TODO el intercambio desde el primer contacto
		 * (PM_SyncMelee, :2532) hasta que se rompe distancia. Estando en melee
		 * (que es la condicion para llegar a esta funcion) apretar TELEPORT es un
		 * no-op silencioso: el boton se presiona, el motor lo descarta, nada pasa.
		 * El zanzoken defensivo real vive en BotLite_RecoverRunFlee (T2.8): la
		 * unica ventana donde el bot NO esta en melee es despues de romper el
		 * lock, que es exactamente cuando huye. */

	if ( BotLite_RunReactiveBlock( bot, clientNum, snapshot, profile, policy ) ) {
		return;
	}

	/* Despues de la defensa: comprometerse a una carga es una decision ofensiva
		 * que reemplaza al combo de ese frame. */
	if ( BotLite_RunDeliberateCharge( bot, clientNum, snapshot ) ) {
		return;
	}

	if ( info->melee.combatState == BOTLITE_COMBAT_STATE_NONE || level.time >= info->melee.combatNextThinkTime ) {
		nextState = BotLite_PickManagedCombatState( bot, clientNum, info, profile, policy, &context, snapshot );
		BotLite_CombatSetState( bot, info, nextState, policy ? policy->tacticalThinkMs : 50 );
	}

	BotLite_RunManagedCombatState( bot, clientNum, target, snapshot, info, policy );
}

void BotLite_RunSkill3MeleePressure( gentity_t *bot, int clientNum, gentity_t *target, const botlite_snapshot_t *snapshot ) {
	BotLite_RunManagedMeleePressure( bot, clientNum, target, snapshot );
}
