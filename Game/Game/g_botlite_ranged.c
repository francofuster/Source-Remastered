#include "g_local.h"
#include "g_botlite.h"

typedef struct {
	int weapon;
	int chargePercent;
	qboolean useAlt;
	qboolean weaponMismatch;
	qboolean charging;
	qboolean readyToRelease;
	qboolean chargeThresholdMet;
	qboolean weaponReady;
	qboolean needsCharge;
	qboolean firing;
	qboolean canStartAttack;
	qboolean staminaAllowsAttack;
} botlite_skill2_pressure_context_t;

typedef enum {
	BOTLITE_SKILL2_PRESSURE_NONE = 0,
	BOTLITE_SKILL2_PRESSURE_WAIT_WEAPON,
	BOTLITE_SKILL2_PRESSURE_RELEASE_CHARGE,
	BOTLITE_SKILL2_PRESSURE_HOLD_CHARGE,
	BOTLITE_SKILL2_PRESSURE_START_ATTACK,
	BOTLITE_SKILL2_PRESSURE_HOLD_FIRE
} botlite_skill2_pressure_decision_t;

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

/*
 * T3.5 -- Economia vida/ki/stamina.
 *
 * Algunas armas tienen costs_health > 0 (g_userweapons.h) -- restan vida propia
 * al usarlas, no solo ki. Con ventaja de vida sobre el rival, skill 3 las deja
 * en el pool de opciones; en desventaja, las descarta mientras haya alternativa,
 * jugando conservador en vez de acelerar su propia derrota.
 */
static qboolean BotLite_WeaponCostsHealth( int clientNum, int weapon ) {
	g_userWeapon_t *weaponData;

	weaponData = G_FindUserWeaponData( clientNum, weapon );
	return ( weaponData && weaponData->costs_health > 0 ) ? qtrue : qfalse;
}

static qboolean BotLite_ShouldAvoidHealthCostWeapons( gentity_t *bot, int skill ) {
	if ( skill != 3 || !bot || !bot->client || !bot->client->ps.lockedPlayer ) {
		return qfalse;
	}
	if ( bot->client->ps.lockedPlayer->powerLevel[plHealth] <= 0 ) {
		return qfalse;
	}
	/* Detras en vida: no sumar mas costo de vida del que ya se esta pagando. */
	return ( bot->client->ps.powerLevel[plHealth] < bot->client->ps.lockedPlayer->powerLevel[plHealth] ) ? qtrue : qfalse;
}

static int BotLite_PickSkill2Weapon( gentity_t *bot, int currentWeapon, int skill ) {
	int i;
	int count;
	int cautiousCount;
	int choices[MAX_PLAYERWEAPONS];
	int cautious[MAX_PLAYERWEAPONS];
	int mask;
	qboolean avoidHealthCost;
	if ( !bot || !bot->client ) {
		return 1;
	}
	avoidHealthCost = BotLite_ShouldAvoidHealthCostWeapons( bot, skill );
	mask = bot->client->ps.stats[stSkills];
	count = 0;
	cautiousCount = 0;
	for ( i = 1; i <= MAX_PLAYERWEAPONS; i++ ) {
		if ( mask & ( 1 << i ) ) {
			if ( BotLite_Skill2BotDisallowsWeapon( bot, i ) ) {
				continue;
			}
			choices[count++] = i;
			if ( avoidHealthCost && !BotLite_WeaponCostsHealth( bot->s.number, i ) ) {
				cautious[cautiousCount++] = i;
			}
		}
	}
	/* Solo se restringe si dejar afuera las que cuestan vida deja algo con que
		 * pelear; sin alternativa, usar lo que haya. */
	if ( avoidHealthCost && cautiousCount > 0 ) {
		return cautious[rand() % cautiousCount];
	}
	if ( count <= 0 ) {
		return 1;
	}
	return choices[rand() % count];
}

static qboolean BotLite_Skill2WeaponHasAlt( int clientNum, int weapon ) {
	g_userWeapon_t *weaponData;
	if ( weapon <= 0 || weapon > MAX_PLAYERWEAPONS ) {
		return qfalse;
	}
	weaponData = G_FindUserWeaponData( clientNum, weapon );
	if ( !weaponData ) {
		return qfalse;
	}
	return ( weaponData->general_bitflags & WPF_ALTWEAPONPRESENT ) ? qtrue : qfalse;
}

static qboolean BotLite_Skill2UseAltAttack( botlite_info_t *info ) {
	return ( info->ranged.attackMode == 2 ) ? qtrue : qfalse;
}

static void BotLite_Skill2PressAttack( gentity_t *bot, qboolean useAlt ) {
	if ( useAlt ) {
		BotLite_EA_Button( bot, BUTTON_ALT_ATTACK );
	} else {
		BotLite_EA_Button( bot, BUTTON_ATTACK );
	}
}

static qboolean BotLite_Skill2ReadyToRelease( gentity_t *bot, qboolean useAlt ) {
	if ( !bot || !bot->client ) {
		return qfalse;
	}
	if ( useAlt ) {
		return ( bot->client->ps.currentSkill[WPSTAT_ALT_BITFLAGS] & WPF_READY ) ? qtrue : qfalse;
	}
	return ( bot->client->ps.currentSkill[WPSTAT_BITFLAGS] & WPF_READY ) ? qtrue : qfalse;
}

static int BotLite_Skill2CurrentChargePercent( gentity_t *bot, qboolean useAlt ) {
	if ( !bot || !bot->client ) {
		return 0;
	}
	return useAlt ? bot->client->ps.stats[stChargePercentSecondary] : bot->client->ps.stats[stChargePercentPrimary];
}

static qboolean BotLite_Skill2NeedsCharge( gentity_t *bot, qboolean useAlt ) {
	if ( !bot || !bot->client ) {
		return qfalse;
	}
	if ( useAlt ) {
		return ( bot->client->ps.currentSkill[WPSTAT_ALT_BITFLAGS] & WPF_NEEDSCHARGE ) ? qtrue : qfalse;
	}
	return ( bot->client->ps.currentSkill[WPSTAT_BITFLAGS] & WPF_NEEDSCHARGE ) ? qtrue : qfalse;
}

static void BotLite_StartSkill2Attack( gentity_t *bot, int clientNum ) {
	botlite_info_t *info;
	qboolean hasAlt;
	const botlite_profile_t *profile;

	info = &g_botlite[clientNum];
	profile = info->profile ? info->profile : BotLite_GetProfile( info->skill );
	info->ranged.weapon = BotLite_PickSkill2Weapon( bot, info->ranged.weapon + 1, info->skill );
	info->ranged.weaponSwitchTime = level.time + profile->skill2WeaponSwitchTime;
	info->ranged.holdUntil = 0;
	hasAlt = BotLite_Skill2WeaponHasAlt( clientNum, info->ranged.weapon );
	if ( info->ranged.weapon == 1 && hasAlt ) {
		info->ranged.attackMode = 2;
	} else {
		info->ranged.attackMode = hasAlt && ( rand() & 1 ) ? 2 : 1;
	}
}

static void BotLite_BuildSkill2PressureContext( gentity_t *bot, int clientNum, botlite_skill2_pressure_context_t *context ) {
	botlite_info_t *info;
	const botlite_profile_t *profile;
	int weaponState;
	int releasePct;

	info = &g_botlite[clientNum];
	profile = info->profile ? info->profile : BotLite_GetProfile( info->skill );
	releasePct = profile ? (int)( profile->rangedChargeReleasePct * 100.0f + 0.5f ) : 80;
	memset( context, 0, sizeof( *context ) );
	if ( info->ranged.weapon <= 0 || info->ranged.weapon > MAX_PLAYERWEAPONS || level.time >= info->ranged.weaponSwitchTime ) {
		BotLite_StartSkill2Attack( bot, clientNum );
	}
	context->weapon = info->ranged.weapon;
	context->useAlt = BotLite_Skill2UseAltAttack( info );
	context->weaponMismatch = ( bot->client->ps.weapon != context->weapon ) ? qtrue : qfalse;
	weaponState = bot->client->ps.weaponstate;
	context->charging = ( context->useAlt && weaponState == WEAPON_ALTCHARGING ) ||
		( !context->useAlt && weaponState == WEAPON_CHARGING );
	context->readyToRelease = context->charging ? BotLite_Skill2ReadyToRelease( bot, context->useAlt ) : qfalse;
	context->weaponReady = ( weaponState == WEAPON_READY ) ? qtrue : qfalse;
	context->needsCharge = BotLite_Skill2NeedsCharge( bot, context->useAlt );
	context->chargePercent = BotLite_Skill2CurrentChargePercent( bot, context->useAlt );
	context->chargeThresholdMet = ( !context->needsCharge || context->chargePercent >= releasePct ) ? qtrue : qfalse;
	context->firing = ( weaponState == WEAPON_FIRING || weaponState == WEAPON_GUIDING ||
		( context->useAlt && ( weaponState == WEAPON_ALTFIRING || weaponState == WEAPON_ALTGUIDING ) ) ) ? qtrue : qfalse;
	context->canStartAttack = ( level.time >= info->melee.nextActionTime ) ? qtrue : qfalse;
	/* T1.3: los ataques de ki cobran fatiga (bg_pmove.c:2779). Sin presupuesto
	 * no se inician nuevos, pero una carga ya empezada si se libera: retenerla
	 * desperdiciaria la energia ya invertida. */
	context->staminaAllowsAttack = BotLite_StaminaAllowsSpend( clientNum, BOTLITE_SPEND_NORMAL );
}

static botlite_skill2_pressure_decision_t BotLite_SelectSkill2PressureDecision( const botlite_skill2_pressure_context_t *context ) {
	if ( context->weaponMismatch ) return BOTLITE_SKILL2_PRESSURE_WAIT_WEAPON;
	if ( context->charging ) {
		if ( context->readyToRelease && context->chargeThresholdMet ) return BOTLITE_SKILL2_PRESSURE_RELEASE_CHARGE;
		return BOTLITE_SKILL2_PRESSURE_HOLD_CHARGE;
	}
	if ( context->weaponReady ) {
		if ( context->canStartAttack && context->staminaAllowsAttack ) return BOTLITE_SKILL2_PRESSURE_START_ATTACK;
		return BOTLITE_SKILL2_PRESSURE_NONE;
	}
	if ( context->needsCharge ) return BOTLITE_SKILL2_PRESSURE_NONE;
	if ( context->firing ) return BOTLITE_SKILL2_PRESSURE_HOLD_FIRE;
	return BOTLITE_SKILL2_PRESSURE_NONE;
}

static void BotLite_ExecuteSkill2PressureDecision( gentity_t *bot, int clientNum, const botlite_skill2_pressure_context_t *context, botlite_skill2_pressure_decision_t decision ) {
	botlite_info_t *info;
	info = &g_botlite[clientNum];
	if ( decision == BOTLITE_SKILL2_PRESSURE_WAIT_WEAPON ) {
		return;
	}
	if ( decision == BOTLITE_SKILL2_PRESSURE_RELEASE_CHARGE ) {
		info->ranged.holdUntil = 0;
		info->melee.nextActionTime = level.time + 350;
		return;
	}
	if ( decision == BOTLITE_SKILL2_PRESSURE_HOLD_CHARGE || decision == BOTLITE_SKILL2_PRESSURE_HOLD_FIRE ) {
		info->ranged.holdUntil = level.time;
		BotLite_Skill2PressAttack( bot, context->useAlt );
		return;
	}
	if ( decision == BOTLITE_SKILL2_PRESSURE_START_ATTACK ) {
		info->ranged.holdUntil = level.time;
		BotLite_Skill2PressAttack( bot, context->useAlt );
		return;
	}
	info->ranged.holdUntil = 0;
}

qboolean BotLite_RunRangedPressure( gentity_t *bot, int clientNum, gentity_t *target, const botlite_snapshot_t *snapshot ) {
	botlite_skill2_pressure_context_t context;
	botlite_skill2_pressure_decision_t decision;
	if ( !bot || !target ) {
		return qfalse;
	}
	BotLite_SetLockOn( bot, target );
	BotLite_FaceTarget( bot, target );
	BotLite_BuildSkill2PressureContext( bot, clientNum, &context );
	BotLite_EA_SetWeapon( bot, context.weapon );
	decision = BotLite_SelectSkill2PressureDecision( &context );
	BotLite_ExecuteSkill2PressureDecision( bot, clientNum, &context, decision );
	return qtrue;
}
