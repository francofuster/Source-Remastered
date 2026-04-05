#include "g_local.h"
#include "g_botlite.h"

static float BotLite_GetHybridMeleeEnterDistance( botlite_info_t *info, const botlite_profile_t *profile ) {
	float meleeEnterDistance;

	if ( !info || !profile ) {
		return 0.0f;
	}

	if ( profile->rangedToMeleeDistance > 0.0f ) {
		meleeEnterDistance = profile->rangedToMeleeDistance;
	} else {
		meleeEnterDistance = profile->skill2RangedDistance * ( 1.15f - info->rangedBias * 0.50f );
	}

	return meleeEnterDistance;
}

static float BotLite_GetHybridMeleeExitDistance( botlite_info_t *info, const botlite_profile_t *profile, float meleeEnterDistance ) {
	float meleeExitDistance;

	if ( !info || !profile ) {
		return meleeEnterDistance + 40.0f;
	}

	if ( profile->meleeToRangedDistance > 0.0f ) {
		meleeExitDistance = profile->meleeToRangedDistance;
	} else {
		meleeExitDistance = profile->skill2MeleeExitDistance * ( 0.90f + info->rangedBias * 0.35f );
	}

	if ( meleeExitDistance < meleeEnterDistance + 40.0f ) {
		meleeExitDistance = meleeEnterDistance + 40.0f;
	}

	return meleeExitDistance;
}

static qboolean BotLite_ShouldUseSanzokenApproachBand( const botlite_combat_policy_t *policy, const botlite_snapshot_t *snapshot, float meleeEnterDistance, float meleeExitDistance ) {
	if ( !policy || !snapshot ) {
		return qfalse;
	}
	if ( !policy->allowsApproachSanzoken ) {
		return qfalse;
	}
	if ( snapshot->botInMelee ) {
		return qfalse;
	}
	if ( !snapshot->hasLineOfSight ) {
		return qfalse;
	}
	if ( snapshot->dist <= meleeEnterDistance ) {
		return qfalse;
	}
	if ( snapshot->dist > meleeExitDistance ) {
		return qfalse;
	}
	return qtrue;
}

static void BotLite_SkillCommonResetRuntime( int clientNum, qboolean respawnStyle ) {
	botlite_info_t *info;

	info = &g_botlite[clientNum];
	if ( respawnStyle ) {
		info->ranged.didSpawnFlyup = qfalse;
	}
}

static qboolean BotLite_CombatCanTransform( gentity_t *bot ) {
	int currentTier;

	if ( !bot || !bot->client ) {
		return qfalse;
	}

	currentTier = bot->client->ps.powerLevel[plTierCurrent];
	if ( currentTier < 0 || currentTier >= 7 ) {
		return qfalse;
	}

	return bot->client->tiers[currentTier + 1].exists ? qtrue : qfalse;
}

static qboolean BotLite_CombatNeedsTransform( gentity_t *bot ) {
	if ( !bot || !bot->client ) {
		return qfalse;
	}
	if ( bot->client->ps.timers[tmTransform] > 0 || bot->client->ps.timers[tmTransform] < 0 ) {
		return qtrue;
	}
	if ( !BotLite_CombatCanTransform( bot ) ) {
		return qfalse;
	}
	if ( bot->client->ps.powerLevel[plTierCurrent] <= 0 ) {
		return qtrue;
	}
	return qfalse;
}

static qboolean BotLite_RunInitialTransform( gentity_t *bot, int clientNum ) {
	botlite_info_t *info;

	info = &g_botlite[clientNum];
	if ( !BotLite_CombatNeedsTransform( bot ) ) {
		info->ranged.didInitialTransform = qtrue;
		if ( !BotLite_CombatCanTransform( bot ) ) {
			BotLite_DebugLog( bot, "Transform skipped: no higher tier" );
		}
		return qfalse;
	}

	BotLite_ClearLock( bot );
	if ( bot->client->ps.timers[tmTransform] == 0 && bot->client->ps.powerLevel[plTierCurrent] <= 0 ) {
		BotLite_EA_Button( bot, BUTTON_POWERLEVEL );
		BotLite_EA_MoveForward( bot, 127 );
		BotLite_DebugLog( bot, "Transforming" );
	} else {
		BotLite_DebugLog( bot, "Waiting transform animation" );
	}
	return qtrue;
}

static qboolean BotLite_RunSearchFlyup( gentity_t *bot, int clientNum, const botlite_snapshot_t *snapshot ) {
	botlite_info_t *info;
	int flyupTime;

	info = &g_botlite[clientNum];
	if ( info->ranged.didSpawnFlyup ) {
		return qfalse;
	}

	flyupTime = info->profile ? info->profile->searchFlyupTime : 2000;
	if ( info->melee.nextActionTime <= 0 ) {
		info->melee.nextActionTime = level.time + flyupTime;
		BotLite_DebugLog( bot, va( "Spawn flyup target=%d dist=%.0f until=%d",
			snapshot && snapshot->target ? snapshot->target->s.number : -1,
			snapshot ? snapshot->dist : 0.0f,
			info->melee.nextActionTime ) );
	}

	if ( level.time < info->melee.nextActionTime ) {
		BotLite_EA_MoveUp( bot, 127 );
		return qtrue;
	}

	info->ranged.didSpawnFlyup = qtrue;
	info->melee.nextActionTime = 0;
	return qfalse;
}

static void BotLite_UpdateHybridRangeMode( gentity_t *bot, int clientNum, const botlite_snapshot_t *snapshot, const botlite_combat_policy_t *policy ) {
	botlite_info_t *info;
	const botlite_profile_t *profile;
	float meleeEnterDistance;
	float meleeExitDistance;

	if ( !snapshot || !policy || !policy->allowsRangedPressure ) {
		return;
	}

	info = &g_botlite[clientNum];
	profile = info->profile ? info->profile : BotLite_GetProfile( info->skill );
	meleeEnterDistance = BotLite_GetHybridMeleeEnterDistance( info, profile );
	meleeExitDistance = BotLite_GetHybridMeleeExitDistance( info, profile, meleeEnterDistance );
	if ( !policy->allowsMeleePressure ) {
		info->ranged.forceMelee = qfalse;
		return;
	}
	if ( snapshot->botInMelee ) {
		info->ranged.forceMelee = qtrue;
	}
	if ( policy->chooseOpeningStyle && policy->directMeleePreference && info->ranged.openingStyle == BOTLITE_SKILL3_OPENING_DIRECT_MELEE ) {
		info->ranged.forceMelee = qtrue;
		return;
	}
	if ( !info->ranged.forceMelee && snapshot->dist <= meleeEnterDistance ) {
		info->ranged.forceMelee = qtrue;
		BotLite_DebugLog( bot, va( "Hybrid enter melee target=%d dist=%.0f", snapshot->target ? snapshot->target->s.number : -1, snapshot->dist ) );
	} else if ( !info->ranged.forceMelee && BotLite_ShouldUseSanzokenApproachBand( policy, snapshot, meleeEnterDistance, meleeExitDistance ) ) {
		info->ranged.forceMelee = qtrue;
		BotLite_DebugLog( bot, va( "Hybrid enter melee-sanzoken band target=%d dist=%.0f", snapshot->target ? snapshot->target->s.number : -1, snapshot->dist ) );
	}
	if ( info->ranged.forceMelee && snapshot->dist > meleeExitDistance && !snapshot->botInMelee ) {
		info->ranged.forceMelee = qfalse;
		info->ranged.weaponSwitchTime = 0;
		BotLite_DebugLog( bot, va( "Hybrid exit melee to ranged target=%d dist=%.0f", snapshot->target ? snapshot->target->s.number : -1, snapshot->dist ) ) ;
	}
}

static void BotLite_ChooseCombatOpening( gentity_t *bot, int clientNum, gentity_t *target, const botlite_combat_policy_t *policy ) {
	botlite_info_t *info;
	int directChance;

	if ( !policy || !policy->chooseOpeningStyle ) {
		return;
	}

	info = &g_botlite[clientNum];
	if ( !target ) {
		info->ranged.openingStyle = BOTLITE_SKILL3_OPENING_NONE;
		info->ranged.openingTargetNum = -1;
		return;
	}
	if ( info->ranged.openingTargetNum == target->s.number && info->ranged.openingStyle != BOTLITE_SKILL3_OPENING_NONE ) {
		return;
	}

	info->ranged.openingTargetNum = target->s.number;
	if ( !policy->directMeleePreference ) {
		info->ranged.openingStyle = BOTLITE_SKILL3_OPENING_RANGED;
		info->ranged.forceMelee = qfalse;
		BotLite_DebugLog( bot, va( "Opening=%s target=%d", "RANGED", target->s.number ) );
		return;
	}
	directChance = 45 + (int)( ( info->aggression - 0.50f ) * 35.0f ) + (int)( ( info->rushTendency - info->rangedBias ) * 35.0f );
	if ( directChance < 15 ) {
		directChance = 15;
	}
	if ( directChance > 85 ) {
		directChance = 85;
	}
	info->ranged.openingStyle = ( ( rand() % 100 ) < directChance ) ? BOTLITE_SKILL3_OPENING_DIRECT_MELEE : BOTLITE_SKILL3_OPENING_RANGED;
	info->ranged.forceMelee = ( info->ranged.openingStyle == BOTLITE_SKILL3_OPENING_DIRECT_MELEE ) ? qtrue : qfalse;
	BotLite_DebugLog( bot, va( "Opening=%s target=%d",
		info->ranged.openingStyle == BOTLITE_SKILL3_OPENING_DIRECT_MELEE ? "DIRECT_MELEE" : "RANGED",
		target->s.number ) );
}

static botlite_tactic_t BotLite_ComputeManagedCombatTactic( gentity_t *bot, int clientNum, const botlite_snapshot_t *snapshot, const botlite_combat_policy_t *policy ) {
	botlite_info_t *info;
	const botlite_profile_t *profile;
	botlite_tactic_t tactic;
	float meleeEnterDistance;

	(void)bot;
	info = &g_botlite[clientNum];
	profile = info->profile ? info->profile : BotLite_GetProfile( info->skill );
	meleeEnterDistance = BotLite_GetHybridMeleeEnterDistance( info, profile );
	if ( !snapshot || !snapshot->hasTarget ) {
		return BOTLITE_TACTIC_IDLE_TRACK;
	}
	if ( snapshot->targetNeedsRecoveryWait ) {
		return BOTLITE_TACTIC_PUNISH_RECOVERY;
	}

	tactic = BOTLITE_TACTIC_MELEE_PRESSURE;
	if ( policy && !policy->allowsMeleePressure ) {
		return policy->allowsRangedPressure ? BOTLITE_TACTIC_RANGED_PRESSURE : BOTLITE_TACTIC_IDLE_TRACK;
	}
	if ( policy && policy->allowsRangedPressure ) {
		if ( policy->chooseOpeningStyle && policy->directMeleePreference && info->ranged.openingStyle == BOTLITE_SKILL3_OPENING_DIRECT_MELEE ) {
			return BOTLITE_TACTIC_MELEE_PRESSURE;
		}
		if ( info->ranged.forceMelee || snapshot->botInMelee || snapshot->dist <= meleeEnterDistance ||
			 BotLite_ShouldUseSanzokenApproachBand( policy, snapshot, meleeEnterDistance, BotLite_GetHybridMeleeExitDistance( info, profile, meleeEnterDistance ) ) ) {
			return BOTLITE_TACTIC_MELEE_PRESSURE;
		}
		tactic = BOTLITE_TACTIC_RANGED_PRESSURE;
	}

	return tactic;
}

void BotLite_RunManagedSearch( gentity_t *bot, int clientNum, const botlite_snapshot_t *snapshot ) {
	botlite_info_t *info;
	const botlite_combat_policy_t *policy;

	info = &g_botlite[clientNum];
	policy = BotLite_GetCombatPolicy( info->skill );

	if ( policy->needsInitialTransform && !info->ranged.didInitialTransform ) {
		if ( BotLite_RunInitialTransform( bot, clientNum ) ) {
			return;
		}
	}

	if ( !snapshot || !snapshot->target ) {
		info->runtime.lastTargetNum = -1;
		BotLite_ResetTransientCombatState( clientNum );
		if ( policy->useSearchPattern ) {
			BotLite_RunSearchPattern( bot, clientNum );
		}
		return;
	}

	info->runtime.lastTargetNum = snapshot->target->s.number;
	if ( policy->useSearchFlyup && BotLite_RunSearchFlyup( bot, clientNum, snapshot ) ) {
		return;
	}

	if ( policy->allowsMeleePressure && policy->chooseOpeningStyle ) {
		BotLite_ChooseCombatOpening( bot, clientNum, snapshot->target, policy );
	} else {
		info->ranged.openingStyle = BOTLITE_SKILL3_OPENING_NONE;
		info->ranged.forceMelee = qfalse;
	}
	info->runtime.mode = BOTLITE_MODE_COMBAT;
	BotLite_UpdateHybridRangeMode( bot, clientNum, snapshot, policy );
	BotLite_RunManagedCombatTactic( bot, clientNum, snapshot, BotLite_SelectManagedCombatTactic( bot, clientNum, snapshot ) );
}

botlite_tactic_t BotLite_SelectManagedCombatTactic( gentity_t *bot, int clientNum, const botlite_snapshot_t *snapshot ) {
	botlite_info_t *info;
	const botlite_combat_policy_t *policy;
	botlite_tactic_t tactic;

	info = &g_botlite[clientNum];
	policy = BotLite_GetCombatPolicy( info->skill );
	if ( !snapshot || !snapshot->hasTarget ) {
		info->runtime.plannedTactic = BOTLITE_TACTIC_IDLE_TRACK;
		info->runtime.nextTacticThinkTime = 0;
		return BOTLITE_TACTIC_IDLE_TRACK;
	}
	if ( info->runtime.plannedTactic != BOTLITE_TACTIC_NONE && level.time < info->runtime.nextTacticThinkTime ) {
		return info->runtime.plannedTactic;
	}

	tactic = BotLite_ComputeManagedCombatTactic( bot, clientNum, snapshot, policy );
	info->runtime.plannedTactic = tactic;
	info->runtime.nextTacticThinkTime = level.time + ( policy ? policy->tacticalThinkMs : 50 );
	return tactic;
}

void BotLite_RunManagedCombatTactic( gentity_t *bot, int clientNum, const botlite_snapshot_t *snapshot, botlite_tactic_t tactic ) {
	botlite_info_t *info;
	const botlite_combat_policy_t *policy;

	if ( !snapshot || !snapshot->target ) {
		return;
	}

	info = &g_botlite[clientNum];
	policy = BotLite_GetCombatPolicy( info->skill );
	BotLite_UpdateHybridRangeMode( bot, clientNum, snapshot, policy );
	if ( policy->allowsMeleePressure && policy->chooseOpeningStyle && policy->directMeleePreference && info->ranged.openingStyle == BOTLITE_SKILL3_OPENING_DIRECT_MELEE ) {
		info->ranged.forceMelee = qtrue;
	}

	switch ( tactic ) {
	case BOTLITE_TACTIC_PUNISH_RECOVERY:
	case BOTLITE_TACTIC_MELEE_PRESSURE:
		BotLite_RunManagedMeleePressure( bot, clientNum, snapshot->target, snapshot );
		break;
	case BOTLITE_TACTIC_RANGED_PRESSURE:
		BotLite_RunRangedPressure( bot, clientNum, snapshot->target, snapshot );
		break;
	case BOTLITE_TACTIC_APPROACH:
		BotLite_SetLockOn( bot, snapshot->target );
		BotLite_FaceTarget( bot, snapshot->target );
		BotLite_EA_MoveForward( bot, 127 );
		BotLite_EA_BoostIfAllowed( bot, clientNum );
		break;
	case BOTLITE_TACTIC_IDLE_TRACK:
	default:
		BotLite_SetLockOn( bot, snapshot->target );
		BotLite_FaceTarget( bot, snapshot->target );
		break;
	}
}

static const botlite_skill_vtable_t botlite_skill1_vtable = {
	"skill1",
	BOTLITE_MODE_SEARCH,
	BotLite_SkillCommonResetRuntime,
	BotLite_RunSkill1Search,
	BotLite_SelectSkill1Tactic,
	BotLite_RunSkill1Tactic
};

static const botlite_skill_vtable_t botlite_skill2_vtable = {
	"skill2",
	BOTLITE_MODE_SEARCH,
	BotLite_SkillCommonResetRuntime,
	BotLite_RunSkill2Search,
	BotLite_SelectSkill2Tactic,
	BotLite_RunSkill2Tactic
};

static const botlite_skill_vtable_t botlite_skill3_vtable = {
	"skill3",
	BOTLITE_MODE_SEARCH,
	BotLite_SkillCommonResetRuntime,
	BotLite_RunSkill3Search,
	BotLite_SelectSkill3Tactic,
	BotLite_RunSkill3Tactic
};

const botlite_skill_vtable_t *BotLite_GetSkillVTable( int skill ) {
	switch ( skill ) {
	case 2:
		return &botlite_skill2_vtable;
	case 3:
		return &botlite_skill3_vtable;
	case 1:
	default:
		return &botlite_skill1_vtable;
	}
}
