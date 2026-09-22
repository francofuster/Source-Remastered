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

static qboolean BotLite_ShouldUseSanzokenApproachBand( const botlite_combat_policy_t *policy, const botlite_snapshot_t *snapshot, float meleeEnterDistance, float meleeExitDistance, float bandMinDist, float bandMaxDist ) {
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
	/* La banda reusaba los umbrales hibridos ranged/melee (10000-12000 en skill3),
		 * que en partida real resulto ser una franja de 2000 unidades que el bot
		 * cruzaba de paso mientras cerraba de ~13000 a ~60: solo 6 usos en toda una
		 * sesion. Ahora tiene banda propia configurable; 0 vuelve al comportamiento
		 * anterior. */
	if ( bandMinDist > 0.0f || bandMaxDist > 0.0f ) {
		if ( bandMinDist > 0.0f && snapshot->dist < bandMinDist ) {
			return qfalse;
		}
		if ( bandMaxDist > 0.0f && snapshot->dist > bandMaxDist ) {
			return qfalse;
		}
		return qtrue;
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
	/* Fase 6: con la capa de intencion activa, forceMelee sale de la decision
	 * y no de los umbrales. La histeresis de abajo queda como respaldo para los
	 * skills que no la tienen habilitada (hoy, skill 1). */
	if ( BotLite_EngageIntentActive( clientNum ) ) {
		if ( !policy->allowsMeleePressure ) {
			info->ranged.forceMelee = qfalse;
			return;
		}
		info->ranged.forceMelee = ( snapshot->botInMelee ||
			BotLite_GetEngageIntent( clientNum ) == BOTLITE_ENGAGE_MELEE ) ? qtrue : qfalse;
		return;
	}
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
	} else if ( !info->ranged.forceMelee && BotLite_ShouldUseSanzokenApproachBand( policy, snapshot, meleeEnterDistance, meleeExitDistance, profile->sanzokenBandMinDist, profile->sanzokenBandMaxDist ) ) {
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
	/* Fase 6: el estilo de apertura ya no fija el modo para toda la pelea.
	 * Siembra la intencion inicial y despues la capa de decision toma el
	 * control, respetando su permanencia minima. */
	if ( BotLite_EngageIntentActive( clientNum ) ) {
		const botlite_profile_t *openProfile;
		openProfile = info->profile ? info->profile : BotLite_GetProfile( info->skill );
		info->engage.intent = ( info->ranged.openingStyle == BOTLITE_SKILL3_OPENING_DIRECT_MELEE )
			? BOTLITE_ENGAGE_MELEE : BOTLITE_ENGAGE_RANGED;
		info->engage.nextSwitchTime = level.time + ( openProfile ? openProfile->engageSwitchMinMs : 0 );
	}
	BotLite_DebugLog( bot, va( "Opening=%s target=%d",
		info->ranged.openingStyle == BOTLITE_SKILL3_OPENING_DIRECT_MELEE ? "DIRECT_MELEE" : "RANGED",
		target->s.number ) );
}

/*
 * T3.3 -- Punish a distancia tras knockback.
 *
 * Un Power melee conectado pone tmKnockback=5000 en el rival y corta el melee del
 * bot que golpeo (PM_StopMelee, bg_pmove.c:2461-2462). La ventana es corta y
 * PARCIALMENTE disputada: por debajo de tmKnockback=4000 el rival puede cancelar
 * el vuelo con ball-flip (ALT_ATTACK, bg_pmove.c:247). O sea: ~1000ms garantizados
 * desde el impacto, el resto es apuesta.
 *
 * La eleccion real de esta tarea es de TIMING, no de intencion: un ataque de carga
 * larga simplemente no sale a tiempo. Se compara el costo de carga del arma
 * elegida (costs_chargeTime * costs_chargeReady, de g_userWeapon_t) contra el
 * margen que queda antes de que tmKnockback baje de 4000, con 300ms de colchon
 * para el vuelo del proyectil y la reaccion.
 *
 * Decision unica por episodio (punishKnockbackDecided): se resetea cuando
 * targetKnockbackTime vuelve a 0, para no re-tirar el dado cada 50ms mientras
 * dura la ventana.
 */
static qboolean BotLite_UpdateKnockbackPunish( gentity_t *bot, int clientNum, botlite_info_t *info, const botlite_snapshot_t *snapshot ) {
	int windowMs;
	int chargeMs;
	int chance;
	g_userWeapon_t *weaponData;

	if ( !snapshot || !snapshot->hasTarget || snapshot->targetKnockbackTime <= 0 ) {
		info->ranged.punishKnockbackDecided = qfalse;
		return qfalse;
	}
	if ( info->ranged.punishKnockbackDecided ) {
		return info->ranged.punishKnockbackUseRanged;
	}

	info->ranged.punishKnockbackDecided = qtrue;
	info->ranged.punishKnockbackUseRanged = qfalse;

	/* Fuera de este rango ya no vale la pena ni evaluarlo: o el golpe fue hace
	 * rato (poco margen) o todavia no sabemos si va a haber ventana real. */
	if ( snapshot->targetKnockbackTime < 4300 ) {
		return qfalse;
	}
	if ( !BotLite_StaminaAllowsSpend( clientNum, BOTLITE_SPEND_NORMAL ) ) {
		return qfalse;
	}

	weaponData = G_FindUserWeaponData( clientNum, info->ranged.weapon );
	if ( !weaponData ) {
		return qfalse;
	}

	chargeMs = 0;
	if ( weaponData->general_bitflags & WPF_NEEDSCHARGE ) {
		chargeMs = weaponData->costs_chargeTime * weaponData->costs_chargeReady;
	}
	/* Margen antes de que tmKnockback cruce el piso de 4000 (cancelable), menos
	 * 300ms de colchon para vuelo de proyectil y reaccion. */
	windowMs = snapshot->targetKnockbackTime - 4000 - 300;
	if ( chargeMs > windowMs ) {
		return qfalse;
	}

	chance = 40 + (int)( info->rangedBias * 40.0f );
	info->ranged.punishKnockbackUseRanged = ( ( rand() % 100 ) < chance ) ? qtrue : qfalse;
	BotLite_DebugLog( bot, va( "Knockback punish: chargeMs=%d windowMs=%d chance=%d%% -> %s",
		chargeMs, windowMs, chance,
		info->ranged.punishKnockbackUseRanged ? "RANGED" : "chase" ) );
	return info->ranged.punishKnockbackUseRanged;
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

	if ( info->skill == 3 && BotLite_UpdateKnockbackPunish( bot, clientNum, info, snapshot ) ) {
		return BOTLITE_TACTIC_RANGED_PRESSURE;
	}

	/* Fase 6: la intencion decide. El bloque de distancia de mas abajo queda
	 * como respaldo para los skills sin capa de intencion. */
	if ( BotLite_EngageIntentActive( clientNum ) ) {
		if ( snapshot->botInMelee ) {
			return BOTLITE_TACTIC_MELEE_PRESSURE;
		}
		if ( BotLite_GetEngageIntent( clientNum ) == BOTLITE_ENGAGE_RANGED ) {
			/* Misma condicion que usa la ejecucion, histeresis incluida: si cada
			 * lado decidiera por su cuenta, la tactica oscilaria en el borde. */
			if ( BotLite_EngageWantsKite( clientNum, snapshot ) ) {
				return BOTLITE_TACTIC_RANGED_KITE;
			}
			return BOTLITE_TACTIC_RANGED_PRESSURE;
		}
		return BOTLITE_TACTIC_MELEE_PRESSURE;
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
			 BotLite_ShouldUseSanzokenApproachBand( policy, snapshot, meleeEnterDistance, BotLite_GetHybridMeleeExitDistance( info, profile, meleeEnterDistance ), profile->sanzokenBandMinDist, profile->sanzokenBandMaxDist ) ) {
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

	/* T2.3: el control de tier corre SIEMPRE, no solo una vez al abrir combate.
		 * Tiene que poder escalar mas tarde y, sobre todo, bajar a voluntad antes de
		 * que el sustain fuerce la des-transformacion. */
	if ( BotLite_RunTransformControl( bot, clientNum, snapshot ) ) {
		return;
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

/*
 * Fase 6 -- cerrar hueco mientras dispara.
 *
 * BotLite_RunRangedPressure apunta y dispara pero no mueve al bot. Con la
 * logica vieja eso casi no se notaba: el modo ranged solo se alcanzaba por
 * encima de 10000 unidades y duraba poco. Ahora que la intencion puede elegir
 * ranged a media distancia y sostenerlo, un bot que no acorta se convierte en
 * un francotirador pasivo. Solo avanza por fuera de la distancia neutral, para
 * no meterse solo en el melee que justamente decidio evitar.
 */
static void BotLite_ApproachWhileShooting( gentity_t *bot, int clientNum, const botlite_snapshot_t *snapshot ) {
	botlite_info_t *info;
	const botlite_profile_t *profile;

	if ( !bot || !snapshot || !BotLite_EngageIntentActive( clientNum ) ) {
		return;
	}
	info = &g_botlite[clientNum];
	profile = info->profile ? info->profile : BotLite_GetProfile( info->skill );
	if ( !profile || profile->engageNeutralDist <= 0.0f ) {
		return;
	}
	if ( snapshot->dist <= profile->engageNeutralDist ) {
		return;
	}
	BotLite_EA_MoveForward( bot, 127 );
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
	if ( !BotLite_EngageIntentActive( clientNum ) &&
		 policy->allowsMeleePressure && policy->chooseOpeningStyle &&
		 policy->directMeleePreference &&
		 info->ranged.openingStyle == BOTLITE_SKILL3_OPENING_DIRECT_MELEE ) {
		info->ranged.forceMelee = qtrue;
	}

	/* La tactica se cachea por tacticalThinkMs. Si en ese intervalo el bot
	 * quedo enganchado en un lock de melee, ejecutar una tactica de ranged seria
	 * un frame perdido: el motor no dispara dentro del lock. */
	if ( snapshot->botInMelee &&
		 ( tactic == BOTLITE_TACTIC_RANGED_PRESSURE || tactic == BOTLITE_TACTIC_RANGED_KITE ) ) {
		tactic = BOTLITE_TACTIC_MELEE_PRESSURE;
	}

	switch ( tactic ) {
	case BOTLITE_TACTIC_PUNISH_RECOVERY:
	case BOTLITE_TACTIC_MELEE_PRESSURE:
		BotLite_RunManagedMeleePressure( bot, clientNum, snapshot->target, snapshot );
		break;
	case BOTLITE_TACTIC_RANGED_KITE:
		/* Si ya abrio hueco suficiente, RunRangedKite devuelve qfalse y este
		 * mismo frame pasa a disparar: no se pierde el tiempo de reaccion. */
		if ( BotLite_RunRangedKite( bot, clientNum, snapshot->target, snapshot ) ) {
			break;
		}
		/* fallthrough */
	case BOTLITE_TACTIC_RANGED_PRESSURE:
		BotLite_RunRangedPressure( bot, clientNum, snapshot->target, snapshot );
		BotLite_ApproachWhileShooting( bot, clientNum, snapshot );
		break;
	case BOTLITE_TACTIC_APPROACH:
		/* Fase 6.3: nivelado, igual que el acercamiento de melee. Avanzar con el
			 * pitch apuntando al rival hace que el bot llegue trepando. */
		BotLite_SetLockOn( bot, snapshot->target );
		BotLite_ApproachTargetLeveled( bot, clientNum, snapshot );
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
