#include "g_local.h"
#include "g_botlite.h"

static float BotLite_RandomRange( float minv, float maxv );
static float BotLite_AngleNormalize360( float angle );
static void BotLite_SetMoveYaw( gentity_t *bot, int clientNum );
static float BotLite_Skill3StatPercent( int value, int maxValue );
static qboolean BotLite_Skill3NeedsHeal( gentity_t *bot, const botlite_profile_t *profile );
static qboolean BotLite_Skill3ReachedHealTarget( gentity_t *bot, const botlite_profile_t *profile );
static qboolean BotLite_Skill3WithinHealRetreatLimit( gentity_t *bot, gentity_t *target, const botlite_profile_t *profile );
static qboolean BotLite_Skill3BotInMeleeNow( gentity_t *bot );
static gentity_t *BotLite_Skill3GetRecentAttacker( gentity_t *bot );
static qboolean BotLite_Skill3IsMeleeAttacker( gentity_t *bot, gentity_t *attacker, const botlite_profile_t *profile );
static qboolean BotLite_Skill3HasForcedCombatEngagement( gentity_t *bot, gentity_t *target );
static qboolean BotLite_Skill3HealInterrupted( gentity_t *bot, botlite_info_t *info, gentity_t *target );
static void BotLite_Skill3PrimeHealTracking( gentity_t *bot, botlite_info_t *info );
static void BotLite_Skill3RefreshHealTracking( gentity_t *bot, botlite_info_t *info );
static qboolean BotLite_RunSkill3RecoveryHeal( gentity_t *bot, int clientNum, gentity_t *target );
static int BotLite_GetActionStateFlags( gentity_t *bot, int clientNum );

static int BotLite_GetActionStateFlags( gentity_t *bot, int clientNum ) {
	if ( !bot || !bot->client || clientNum < 0 || clientNum >= level.maxclients ) {
		return 0;
	}
	return trap_BotQueryActionState( clientNum );
}

qboolean BotLite_ShouldUseBoost( gentity_t *bot, int clientNum ) {
	botlite_info_t *info;
	int actionFlags;

	if ( !bot || !bot->client || clientNum < 0 || clientNum >= level.maxclients ) {
		return qfalse;
	}

	info = &g_botlite[clientNum];
	actionFlags = BotLite_GetActionStateFlags( bot, clientNum );

	if ( info->runtime.mode == BOTLITE_MODE_WAIT_TARGET_RECOVERY ) {
		return qfalse;
	}

	/*
	 * Fase 6.5: aca habia ademas un "return qfalse si !didInitialTransform".
	 * Era un resto del modelo viejo, donde skill 3 se transformaba una vez al
	 * abrir combate y recien despues se le permitia lo demas. T2.3 reemplazo eso
	 * por gestion continua de tiers y el flag dejo de tener un momento claro en
	 * que encenderse: solo se pone en qtrue cuando el bot ya llego a su tier
	 * maximo.
	 *
	 * Mientras el bot escalaba a los golpes eso pasaba desapercibido, porque
	 * terminaba llegando al tope y desbloqueaba todo. Al limitar los intentos de
	 * transformacion (Fase 6.4) el bot se quedo en tier 0, el flag nunca se
	 * encendio y esto dejo sin boost NI sanzoken a skill 3 durante toda la
	 * partida. Reportado como "ahora nunca se transforma ni hace ki boost".
	 */
	if ( !( actionFlags & BOTACT_CAN_BOOST ) ) {
		if ( level.time >= info->runtime.boostDenyLogTime ) {
			info->runtime.boostDenyLogTime = level.time + 5000;
			BotLite_DebugLog( bot, "Boost bloqueado: el tier actual no habilita canBoost" );
		}
		return qfalse;
	}

	{
		/* Fase 6.7: el diagnostico anterior solo cubria el presupuesto de stamina,
			 * que en el log nunca disparo ni una vez. Van tres reportes de "no hace
			 * boost" sin poder decir por que. Ahora se nombra el primer gate que corta. */
		const char *deny = NULL;
		if ( actionFlags & BOTACT_TRANSFORMING ) { deny = "TRANSFORMING"; }
		else if ( actionFlags & BOTACT_CHARGING ) { deny = "CHARGING"; }
		else if ( actionFlags & BOTACT_USING_MELEE ) { deny = "USING_MELEE"; }
		else if ( actionFlags & BOTACT_USING_WEAPON ) { deny = "USING_WEAPON"; }
		else if ( actionFlags & BOTACT_WEAPON_BUSY ) { deny = "WEAPON_BUSY"; }
		else if ( actionFlags & BOTACT_USING_ZANZOKEN ) { deny = "USING_ZANZOKEN"; }
		else if ( actionFlags & BOTACT_MELEE_RECOVERY ) { deny = "MELEE_RECOVERY"; }
		else if ( actionFlags & BOTACT_USING_BLOCK ) { deny = "USING_BLOCK"; }
		else if ( actionFlags & BOTACT_FREEZE ) { deny = "FREEZE"; }
		else if ( actionFlags & BOTACT_KNOCKBACK ) { deny = "KNOCKBACK"; }
		else if ( actionFlags & BOTACT_RECOVERING ) { deny = "RECOVERING"; }
		else if ( actionFlags & BOTACT_USING_SOAR ) { deny = "USING_SOAR"; }
		else if ( actionFlags & BOTACT_PREPARING ) { deny = "PREPARING"; }
		else if ( actionFlags & BOTACT_STRUGGLING ) { deny = "STRUGGLING"; }
		else if ( actionFlags & BOTACT_GUIDING ) { deny = "GUIDING"; }
		else if ( actionFlags & BOTACT_CRASHED ) { deny = "CRASHED"; }
		else if ( actionFlags & BOTACT_UNCONSCIOUS ) { deny = "UNCONSCIOUS"; }
		else if ( info->melee.actionUntil > level.time ) { deny = "combo en curso"; }
		else if ( info->melee.blockUntil > level.time ) { deny = "bloqueando"; }
		else if ( info->melee.specialHoldUntil > level.time ) { deny = "special en curso"; }
		if ( deny ) {
			if ( level.time >= info->runtime.boostDenyLogTime ) {
				info->runtime.boostDenyLogTime = level.time + 5000;
				BotLite_DebugLog( bot, va( "Boost bloqueado: %s", deny ) );
			}
			return qfalse;
		}
	}

	/* T0.3: con stamina real, dejar de boostear antes de vaciarse. */
	if ( !BotLite_StaminaAllowsSpend( clientNum, BOTLITE_SPEND_CHEAP ) ) {
		if ( level.time >= info->runtime.boostDenyLogTime ) {
			info->runtime.boostDenyLogTime = level.time + 5000;
			BotLite_DebugLog( bot, va( "Boost bloqueado: presupuesto de stamina (%d%%)",
				BotLite_StaminaPercent( clientNum ) ) );
		}
		return qfalse;
	}

	return qtrue;
}

qboolean BotLite_ShouldUseSanzoken( gentity_t *bot, int clientNum ) {
	botlite_info_t *info;
	int actionFlags;

	if ( !bot || !bot->client || clientNum < 0 || clientNum >= level.maxclients ) {
		return qfalse;
	}

	info = &g_botlite[clientNum];
	actionFlags = BotLite_GetActionStateFlags( bot, clientNum );

	if ( info->runtime.mode == BOTLITE_MODE_WAIT_TARGET_RECOVERY ) {
		return qfalse;
	}

	if ( !( actionFlags & BOTACT_CAN_ZANZOKEN ) ) {
		return qfalse;
	}

	/*
	 * Fase 6.5: aca habia ademas un "return qfalse si !didInitialTransform".
	 * Era un resto del modelo viejo, donde skill 3 se transformaba una vez al
	 * abrir combate y recien despues se le permitia lo demas. T2.3 reemplazo eso
	 * por gestion continua de tiers y el flag dejo de tener un momento claro en
	 * que encenderse: solo se pone en qtrue cuando el bot ya llego a su tier
	 * maximo.
	 *
	 * Mientras el bot escalaba a los golpes eso pasaba desapercibido, porque
	 * terminaba llegando al tope y desbloqueaba todo. Al limitar los intentos de
	 * transformacion (Fase 6.4) el bot se quedo en tier 0, el flag nunca se
	 * encendio y esto dejo sin boost NI sanzoken a skill 3 durante toda la
	 * partida. Reportado como "ahora nunca se transforma ni hace ki boost".
	 */
	if ( info->skill == 3 && ( actionFlags & BOTACT_TRANSFORMING ) ) {
		return qfalse;
	}

	/* Keep this helper permissive for approach teleports. Hard-disabled states still block,
	 * but short transitional flags from ranged pressure should not kill sanzoken entirely. */
	if ( actionFlags & ( BOTACT_FREEZE |
			BOTACT_KNOCKBACK |
			BOTACT_RECOVERING |
			BOTACT_TRANSFORMING |
			BOTACT_USING_MELEE |
			BOTACT_USING_ZANZOKEN |
			BOTACT_MELEE_RECOVERY |
			BOTACT_CRASHED |
			BOTACT_UNCONSCIOUS ) ) {
		return qfalse;
	}

	if ( info->melee.blockUntil > level.time ||
		 info->melee.specialHoldUntil > level.time ) {
		return qfalse;
	}

	/* T0.3: el zanzoken cuesta plMaximum*0.12 (bg_pmove.c:331) y se bloquea
	 * solo con fatiga <= 1; cortarlo antes de llegar a ese piso. */
	if ( !BotLite_StaminaAllowsSpend( clientNum, BOTLITE_SPEND_EXPENSIVE ) ) {
		return qfalse;
	}

	return qtrue;
}

void BotLite_EA_BoostIfAllowed( gentity_t *bot, int clientNum ) {
	if ( BotLite_ShouldUseBoost( bot, clientNum ) ) {
		BotLite_EA_Button( bot, BUTTON_BOOST );
	}
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

float BotLite_ShortestAngleDelta( float from, float to ) {
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
	const botlite_profile_t *profile;

	info = &g_botlite[clientNum];
	profile = info->profile ? info->profile : BotLite_GetProfile( info->skill );
	currentYaw = bot->client->ps.viewangles[YAW];
	yawDelta = BotLite_RandomRange( profile->searchTurnMin, profile->searchTurnMax );
	if ( rand() & 1 ) {
		yawDelta = -yawDelta;
	}
	info->search.turnYawStart = BotLite_AngleNormalize360( currentYaw );
	info->search.moveYaw = BotLite_AngleNormalize360( currentYaw + yawDelta );
	info->search.turnEndTime = level.time + profile->searchTurnTime;
	info->search.moveEndTime = info->search.turnEndTime + profile->searchForwardTime;
}

static float BotLite_Skill3StatPercent( int value, int maxValue ) {
	if ( maxValue <= 0 ) {
		return 1.0f;
	}
	return (float)value / (float)maxValue;
}

static qboolean BotLite_Skill3NeedsHeal( gentity_t *bot, const botlite_profile_t *profile ) {
	float healthPct;
	if ( !bot || !bot->client || !profile ) {
		return qfalse;
	}
	healthPct = BotLite_Skill3StatPercent( bot->client->ps.powerLevel[plHealth], bot->client->ps.powerLevel[plMaximum] );
	return ( healthPct < profile->skill3HealthStartPct ) ? qtrue : qfalse;
}

static qboolean BotLite_Skill3ReachedHealTarget( gentity_t *bot, const botlite_profile_t *profile ) {
	float healthPct;
	if ( !bot || !bot->client || !profile ) {
		return qtrue;
	}
	healthPct = BotLite_Skill3StatPercent( bot->client->ps.powerLevel[plHealth], bot->client->ps.powerLevel[plMaximum] );
	return ( healthPct >= profile->skill3HealthStopPct ) ? qtrue : qfalse;
}

static qboolean BotLite_Skill3WithinHealRetreatLimit( gentity_t *bot, gentity_t *target, const botlite_profile_t *profile ) {
	vec3_t delta;
	float distSq;
	float maxDist;

	if ( !target || !target->client || !profile ) {
		return qfalse;
	}
	maxDist = profile->skill3HealRetreatMaxDistance;
	if ( maxDist <= 0.0f ) {
		return qtrue;
	}
	VectorSubtract( target->client->ps.origin, bot->client->ps.origin, delta );
	distSq = VectorLengthSquared( delta );
	return ( distSq < maxDist * maxDist ) ? qtrue : qfalse;
}

static qboolean BotLite_Skill3BotInMeleeNow( gentity_t *bot ) {
	if ( !bot || !bot->client ) {
		return qfalse;
	}
	if ( bot->client->ps.bitFlags & usingMelee ) {
		return qtrue;
	}
	return ( bot->client->ps.stats[stMeleeState] != 0 ) ? qtrue : qfalse;
}

static gentity_t *BotLite_Skill3GetRecentAttacker( gentity_t *bot ) {
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
	if ( attackerNum == bot->s.number ) {
		return NULL;
	}

	attacker = &g_entities[attackerNum];
	if ( !BotLite_TargetIsValid( bot, attacker ) ) {
		return NULL;
	}

	return attacker;
}

static qboolean BotLite_Skill3IsMeleeAttacker( gentity_t *bot, gentity_t *attacker, const botlite_profile_t *profile ) {
	vec3_t delta;
	float distSq;
	float meleeRange;
	qboolean attackerMelee;
	qboolean botMeleeLink;

	if ( !bot || !bot->client || !attacker || !attacker->client ) {
		return qfalse;
	}

	attackerMelee = ( attacker->client->ps.bitFlags & usingMelee ) ? qtrue : qfalse;
	if ( attacker->client->ps.stats[stMeleeState] != 0 ) {
		attackerMelee = qtrue;
	}

	botMeleeLink = qfalse;
	if ( attacker->client->ps.lockedTarget == bot->s.number + 1 || bot->client->ps.lockedTarget == attacker->s.number + 1 ) {
		botMeleeLink = qtrue;
	}

	VectorSubtract( attacker->client->ps.origin, bot->client->ps.origin, delta );
	distSq = VectorLengthSquared( delta );
	meleeRange = profile ? profile->lockRange : 2200.0f;
	if ( meleeRange < 12000.0f ) {
		meleeRange = 12000.0f;
	}

	if ( distSq <= meleeRange * meleeRange && ( attackerMelee || botMeleeLink ) ) {
		return qtrue;
	}

	if ( botMeleeLink && ( bot->client->ps.timers[tmKnockback] > 0 || bot->client->ps.timers[tmFreeze] > 0 ) ) {
		return qtrue;
	}

	return qfalse;
}


static void BotLite_Skill3PrimeHealTracking( gentity_t *bot, botlite_info_t *info ) {
	if ( !bot || !bot->client || !info ) {
		return;
	}
	info->runtime.lastDamageEvent = bot->client->ps.damageEvent;
	info->recovery.healLastHealth = bot->client->ps.powerLevel[plHealth];
	info->recovery.healLastCurrent = bot->client->ps.powerLevel[plCurrent];
	info->recovery.healLastProgressTime = level.time;
}

static void BotLite_Skill3RefreshHealTracking( gentity_t *bot, botlite_info_t *info ) {
	if ( !bot || !bot->client || !info ) {
		return;
	}
	if ( bot->client->ps.powerLevel[plHealth] > info->recovery.healLastHealth ||
		 bot->client->ps.powerLevel[plCurrent] > info->recovery.healLastCurrent ) {
		info->recovery.healLastProgressTime = level.time;
	}
	info->recovery.healLastHealth = bot->client->ps.powerLevel[plHealth];
	info->recovery.healLastCurrent = bot->client->ps.powerLevel[plCurrent];
}
static qboolean BotLite_Skill3HasForcedCombatEngagement( gentity_t *bot, gentity_t *target ) {
	if ( !bot || !bot->client || !target || !target->client ) {
		return qfalse;
	}

	if ( bot->client->ps.lockedTarget == target->s.number + 1 ||
		 target->client->ps.lockedTarget == bot->s.number + 1 ) {
		return qtrue;
	}

	return qfalse;
}

static qboolean BotLite_Skill3HealInterrupted( gentity_t *bot, botlite_info_t *info, gentity_t *target ) {
	gentity_t *attacker;
	qboolean forcedCombatInterrupt;

	if ( !bot || !bot->client || !info ) {
		return qfalse;
	}

	forcedCombatInterrupt = BotLite_Skill3HasForcedCombatEngagement( bot, target );
	if ( forcedCombatInterrupt ) {
		BotLite_DebugLog( bot, "Skill3 heal interrupted by forced lockOn" );
		return qtrue;
	}

	if ( bot->client->ps.damageCount <= 0 ) {
		return qfalse;
	}
	if ( bot->client->ps.damageEvent == info->runtime.lastDamageEvent ) {
		return qfalse;
	}

	attacker = BotLite_Skill3GetRecentAttacker( bot );
	info->runtime.lastDamageEvent = bot->client->ps.damageEvent;
	if ( attacker && target && attacker->s.number == target->s.number ) {
		BotLite_DebugLog( bot, va( "Skill3 heal interrupted by tracked target hit attacker=%d dmgEv=%d", attacker->s.number, bot->client->ps.damageEvent ) );
		return qtrue;
	}

	if ( attacker && attacker->client ) {
		BotLite_DebugLog( bot, va( "Skill3 heal ignored off-target damage attacker=%d dmgEv=%d", attacker->s.number, bot->client->ps.damageEvent ) );
	} else {
		BotLite_DebugLog( bot, va( "Skill3 heal ignored non-target damage dmgEv=%d", bot->client->ps.damageEvent ) );
	}
	return qfalse;
}

void BotLite_StartSkill3DeathHeal( int clientNum ) {
	botlite_info_t *info;
	gentity_t *bot;
	const botlite_profile_t *profile;

	if ( clientNum < 0 || clientNum >= level.maxclients ) {
		return;
	}
	info = &g_botlite[clientNum];
	if ( info->skill != 3 ) {
		return;
	}
	bot = &g_entities[clientNum];
	profile = info->profile ? info->profile : BotLite_GetProfile( info->skill );
	if ( !BotLite_Skill3NeedsHeal( bot, profile ) ) {
		return;
	}
	info->runtime.mode = BOTLITE_MODE_WAIT_TARGET_RECOVERY;
	info->runtime.lastTargetNum = -1;
	info->recovery.retreatUntil = 0;
	info->recovery.healRequested = qtrue;
	info->recovery.healActive = qfalse;
	BotLite_Skill3PrimeHealTracking( bot, info );
	BotLite_ResetTransientCombatState( clientNum );
	BotLite_ClearLock( bot );
	BotLite_DebugLog( bot, "Skill3 start heal window on target death" );
}

qboolean BotLite_RunPostCrashFlyup( gentity_t *bot, int clientNum ) {
	botlite_info_t *info;
	const botlite_profile_t *profile;

	info = &g_botlite[clientNum];
	profile = info->profile ? info->profile : BotLite_GetProfile( info->skill );
	if ( !info->recovery.postCrashFlyup ) {
		return qfalse;
	}

	if ( info->recovery.postCrashRiseEndTime <= 0 ) {
		info->recovery.postCrashRiseEndTime = level.time + profile->postCrashFlyupTime;
	}

	if ( level.time < info->recovery.postCrashRiseEndTime ) {
		BotLite_EA_MoveUp( bot, 127 );
		return qtrue;
	}

	BotLite_DebugLog( bot, va( "Finish post-crash flyup lastTarget=%d", info->runtime.lastTargetNum ) );
	info->recovery.postCrashFlyup = qfalse;
	info->recovery.postCrashRiseEndTime = 0;
	info->search.didInitialRise = qtrue;
	info->runtime.mode = ( info->runtime.lastTargetNum >= 0 ) ? BOTLITE_MODE_COMBAT : BotLite_DefaultModeForSkill( info->skill );
	BotLite_ResetTransientCombatState( clientNum );
	return qfalse;
}

void BotLite_StartRecoveryWait( gentity_t *bot, int clientNum, gentity_t *target ) {
	botlite_info_t *info;
	vec3_t delta;
	const botlite_profile_t *profile;

	info = &g_botlite[clientNum];
	profile = info->profile ? info->profile : BotLite_GetProfile( info->skill );
	info->runtime.mode = BOTLITE_MODE_WAIT_TARGET_RECOVERY;
	info->runtime.lastTargetNum = target ? target->s.number : -1;
	BotLite_ResetTransientCombatState( clientNum );
	info->recovery.retreatUntil = level.time + profile->recoveryRetreatTime;
	info->recovery.recoveryWaitEndTime = 0;
	info->recovery.healRequested = ( info->skill == 3 && BotLite_Skill3NeedsHeal( bot, profile ) ) ? qtrue : qfalse;
	info->recovery.healActive = qfalse;
	if ( info->recovery.healRequested ) {
		BotLite_Skill3PrimeHealTracking( bot, info );
	}
	BotLite_DebugLog( bot, va( "Start retreat target=%d retreatUntil=%d heal=%d", info->runtime.lastTargetNum, info->recovery.retreatUntil, info->recovery.healRequested ? 1 : 0 ) );

	if ( target && target->client ) {
		VectorSubtract( bot->client->ps.origin, target->client->ps.origin, delta );
		if ( delta[0] == 0.0f && delta[1] == 0.0f ) {
			info->recovery.retreatYaw = bot->client->ps.viewangles[YAW];
		} else {
			info->recovery.retreatYaw = vectoyaw( delta );
		}
	} else {
		info->recovery.retreatYaw = bot->client->ps.viewangles[YAW];
	}
}

static qboolean BotLite_RunSkill3RecoveryHeal( gentity_t *bot, int clientNum, gentity_t *target ) {
	botlite_info_t *info;
	const botlite_profile_t *profile;

	info = &g_botlite[clientNum];
	profile = info->profile ? info->profile : BotLite_GetProfile( info->skill );
	if ( info->skill != 3 ) {
		return qfalse;
	}
	if ( !info->recovery.healRequested && !info->recovery.healActive ) {
		return qfalse;
	}
	BotLite_Skill3RefreshHealTracking( bot, info );
	if ( BotLite_Skill3HealInterrupted( bot, info, target ) ) {
		info->recovery.healRequested = qfalse;
		info->recovery.healActive = qfalse;
			info->recovery.healLastProgressTime = 0;
		info->runtime.mode = BOTLITE_MODE_COMBAT;
		BotLite_ResetTransientCombatState( clientNum );
		BotLite_DebugLog( bot, "Skill3 heal interrupted by damage" );
		return qfalse;
	}
	if ( BotLite_Skill3ReachedHealTarget( bot, profile ) ) {
		info->recovery.healRequested = qfalse;
		info->recovery.healActive = qfalse;
			info->recovery.healLastProgressTime = 0;
		BotLite_DebugLog( bot, "Skill3 heal target reached" );
		return qfalse;
	}
	if ( info->recovery.healLastProgressTime > 0 && ( level.time - info->recovery.healLastProgressTime ) > 5000 ) {
		info->recovery.healRequested = qfalse;
		info->recovery.healActive = qfalse;
			BotLite_DebugLog( bot, "Skill3 heal stalled -> abort window" );
		return qfalse;
	}
	info->recovery.healRequested = qfalse;
	info->recovery.healActive = qtrue;
	if ( target && target->client ) {
		BotLite_FaceTarget( bot, target );
	}
	/* POWERLEVEL + forwardmove>0 es 'subir de tier' (bg_pmove.c:694). Como la
	 * accion se acumula durante el frame, cualquier movimiento hacia adelante que
	 * haya quedado seteado antes convertiria esta carga de ki en una
	 * transformacion involuntaria. Se limpia de forma explicita. */
	BotLite_EA_MoveForward( bot, 0 );
	BotLite_EA_Button( bot, BUTTON_POWERLEVEL );
	BotLite_EA_MoveRight( bot, 127 );
	return qtrue;
}

qboolean BotLite_RunRecoveryWait( gentity_t *bot, int clientNum, gentity_t *target ) {
	botlite_info_t *info;
	vec3_t angles;
	qboolean targetNeedsRecovery;

	info = &g_botlite[clientNum];
	if ( info->runtime.mode != BOTLITE_MODE_WAIT_TARGET_RECOVERY ) {
		return qfalse;
	}

	targetNeedsRecovery = ( target && target->client ) ? BotLite_TargetNeedsRecoveryWait( target ) : qfalse;

	if ( level.time < info->recovery.retreatUntil ) {
		VectorClear( angles );
		angles[YAW] = info->recovery.retreatYaw;
		BotLite_ApplyViewAngles( bot, angles );
		if ( BotLite_Skill3WithinHealRetreatLimit( bot, target, info->profile ? info->profile : BotLite_GetProfile( info->skill ) ) ) {
			BotLite_EA_MoveForward( bot, 127 );
		}
		return qtrue;
	}

	if ( info->recovery.retreatUntil != 0 ) {
		BotLite_DebugLog( bot, va( "Retreat finished, target=%d recovered=%d", info->runtime.lastTargetNum, targetNeedsRecovery ? 0 : 1 ) );
		info->recovery.retreatUntil = 0;
	}

	if ( BotLite_RunSkill3RecoveryHeal( bot, clientNum, target ) ) {
		return qtrue;
	}

	if ( target && target->client && targetNeedsRecovery ) {
		BotLite_FaceTarget( bot, target );
		return qtrue;
	}

	BotLite_DebugLog( bot, va( "Recovery finished -> reengage target=%d", info->runtime.lastTargetNum ) );
	info->runtime.mode = ( target && target->client ) ? BOTLITE_MODE_COMBAT : BotLite_DefaultModeForSkill( info->skill );
	BotLite_ResetTransientCombatState( clientNum );
	info->recovery.retreatUntil = 0;
	info->recovery.recoveryWaitEndTime = 0;
	info->recovery.healRequested = qfalse;
	info->recovery.healActive = qfalse;
	info->recovery.healLastProgressTime = 0;

	if ( target && target->client ) {
		info->runtime.lastTargetNum = target->s.number;
		BotLite_SetLockOn( bot, target );
	} else {
		info->runtime.lastTargetNum = -1;
	}

	return qfalse;
}

void BotLite_RunSearchPattern( gentity_t *bot, int clientNum ) {
	botlite_info_t *info;
	const botlite_profile_t *profile;
	vec3_t angles;
	float yaw;

	info = &g_botlite[clientNum];
	profile = info->profile ? info->profile : BotLite_GetProfile( info->skill );

	if ( !info->search.didInitialRise ) {
		info->search.didInitialRise = qtrue;
		info->search.riseEndTime = level.time + profile->searchRiseTime;
		info->search.turnEndTime = 0;
		info->search.moveEndTime = 0;
	}

	if ( level.time < info->search.riseEndTime ) {
		BotLite_EA_MoveUp( bot, 127 );
		return;
	}

	if ( level.time >= info->search.moveEndTime ) {
		BotLite_SetMoveYaw( bot, clientNum );
	}

	if ( level.time < info->search.turnEndTime ) {
		float frac;
		float delta;
		frac = (float)( level.time - ( info->search.turnEndTime - profile->searchTurnTime ) ) / (float)profile->searchTurnTime;
		if ( frac < 0.0f ) frac = 0.0f;
		if ( frac > 1.0f ) frac = 1.0f;
		delta = BotLite_ShortestAngleDelta( info->search.turnYawStart, info->search.moveYaw );
		yaw = BotLite_AngleNormalize360( info->search.turnYawStart + delta * frac );
	} else {
		yaw = info->search.moveYaw;
		BotLite_EA_MoveForward( bot, 127 );
	}

	VectorClear( angles );
	angles[YAW] = yaw;
	BotLite_ApplyViewAngles( bot, angles );
}

void BotLite_RunDefaultSearch( gentity_t *bot, int clientNum, gentity_t *target ) {
	if ( target ) {
		g_botlite[clientNum].runtime.mode = BOTLITE_MODE_COMBAT;
		g_botlite[clientNum].runtime.lastTargetNum = target->s.number;
		BotLite_DebugLog( bot, "Default search acquired target" );
		BotLite_SetLockOn( bot, target );
		return;
	}

	BotLite_RunSearchPattern( bot, clientNum );
}
