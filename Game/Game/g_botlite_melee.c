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
		BotLite_EA_Button( bot, BUTTON_ATTACK );
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

static void BotLite_BeginMeleePressure( gentity_t *bot, int clientNum, gentity_t *target ) {
	(void)clientNum;
	BotLite_SetLockOn( bot, target );
	BotLite_FaceTarget( bot, target );
}

static void BotLite_ApplyApproachMovement( gentity_t *bot, int clientNum ) {
	BotLite_EA_MoveForward( bot, 127 );
	BotLite_EA_BoostIfAllowed( bot, clientNum );
}

static void BotLite_BuildMeleeContext( gentity_t *bot, int clientNum, const botlite_snapshot_t *snapshot, botlite_melee_context_t *context ) {
	botlite_info_t *info;
	const botlite_profile_t *profile;

	info = &g_botlite[clientNum];
	profile = info->profile ? info->profile : BotLite_GetProfile( info->skill );
	memset( context, 0, sizeof( *context ) );
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
		BotLite_EA_Button( bot, BUTTON_ATTACK );
		info->melee.actionUntil = level.time + 90;
	} else if ( info->melee.comboStage == BOTLITE_COMBO_KICK ) {
		BotLite_EA_Button( bot, BUTTON_ALT_ATTACK );
		info->melee.actionUntil = level.time + 90;
	} else if ( info->melee.comboStage == BOTLITE_COMBO_SPEED ) {
		info->melee.actionUntil = level.time + 240;
	} else {
		BotLite_EA_Button( bot, BUTTON_ALT_ATTACK );
		info->melee.actionUntil = level.time + 650;
	}
}

static void BotLite_ExecuteMeleeDecision( gentity_t *bot, int clientNum, botlite_info_t *info, const botlite_melee_context_t *context, botlite_melee_decision_t decision ) {
	if ( decision == BOTLITE_MELEE_DECISION_CHASE ) {
		BotLite_ResetMeleeSequence( clientNum );
		BotLite_ApplyApproachMovement( bot, clientNum );
		return;
	}
	if ( decision == BOTLITE_MELEE_DECISION_WAIT_LATCH ) {
		if ( context->canPrimeLatch ) {
			BotLite_ResetMeleeSequence( clientNum );
		}
		BotLite_ApplyApproachMovement( bot, clientNum );
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
	BotLite_BeginMeleePressure( bot, clientNum, target );
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
	BotLite_EA_Button( bot, BUTTON_TELEPORT );
	BotLite_EA_MoveForward( bot, 127 );
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
		BotLite_ApplyApproachMovement( bot, clientNum );
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
	BotLite_BeginMeleePressure( bot, clientNum, target );
	BotLite_BuildMeleeContext( bot, clientNum, snapshot, &context );
	BotLite_CombatClearTransientDodgeState( info );

	if ( info->melee.combatState == BOTLITE_COMBAT_STATE_NONE || level.time >= info->melee.combatNextThinkTime ) {
		nextState = BotLite_PickManagedCombatState( bot, clientNum, info, profile, policy, &context, snapshot );
		BotLite_CombatSetState( bot, info, nextState, policy ? policy->tacticalThinkMs : 50 );
	}

	BotLite_RunManagedCombatState( bot, clientNum, target, snapshot, info, policy );
}

void BotLite_RunSkill3MeleePressure( gentity_t *bot, int clientNum, gentity_t *target, const botlite_snapshot_t *snapshot ) {
	BotLite_RunManagedMeleePressure( bot, clientNum, target, snapshot );
}
