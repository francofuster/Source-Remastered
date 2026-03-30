#include "g_local.h"
#include "g_botlite.h"

static void BotLite_StartCombo( int clientNum );
static void BotLite_AdvanceCombo( int clientNum );
static void BotLite_ApplyComboButtons( botlite_info_t *info, usercmd_t *cmd );
static int BotLite_PickSkill2Weapon( gentity_t *bot, int currentWeapon );
static void BotLite_StartSkill2Attack( gentity_t *bot, int clientNum );
static void BotLite_RunSkill2RangedPressure( gentity_t *bot, int clientNum, gentity_t *target );
static qboolean BotLite_Skill2WeaponHasAlt( int clientNum, int weapon );
static qboolean BotLite_Skill2UseAltAttack( botlite_info_t *info );
static void BotLite_Skill2PressAttack( usercmd_t *cmd, qboolean useAlt );
static qboolean BotLite_Skill2ReadyToRelease( gentity_t *bot, qboolean useAlt );
static qboolean BotLite_Skill2NeedsCharge( gentity_t *bot, qboolean useAlt );

static void BotLite_StartCombo( int clientNum ) {
	botlite_info_t *info;
	info = &g_botlite[clientNum];
	info->comboStage = BOTLITE_COMBO_PUNCH + ( rand() % 2 );
	info->comboStep = 0;
	info->nextActionTime = level.time;
	info->actionUntil = 0;
	BotLite_DebugLog( &g_entities[clientNum], "Combo started" );
}


static void BotLite_AdvanceCombo( int clientNum ) {
	botlite_info_t *info;
	info = &g_botlite[clientNum];

	if ( info->comboStage == BOTLITE_COMBO_PUNCH || info->comboStage == BOTLITE_COMBO_KICK ) {
		info->comboStep++;
		if ( info->comboStep >= 2 + ( rand() % 2 ) ) {
			info->comboStep = 0;
			info->comboStage = BOTLITE_COMBO_SPEED;
		} else {
			info->comboStage = ( info->comboStage == BOTLITE_COMBO_PUNCH ) ? BOTLITE_COMBO_KICK : BOTLITE_COMBO_PUNCH;
		}
	}
	else if ( info->comboStage == BOTLITE_COMBO_SPEED ) {
		info->comboStage = BOTLITE_COMBO_FINISH;
	}
	else {
		info->comboStage = BOTLITE_COMBO_PUNCH + ( rand() % 2 );
		info->comboStep = 0;
	}

	info->nextActionTime = level.time + 110 + ( rand() % 90 );
	BotLite_DebugLog( &g_entities[clientNum], "Combo advanced" );
}


static void BotLite_ApplyComboButtons( botlite_info_t *info, usercmd_t *cmd ) {
	cmd->forwardmove = 127;
	cmd->buttons |= BUTTON_BOOST;

	switch ( info->comboStage ) {
	case BOTLITE_COMBO_PUNCH:
		cmd->buttons |= BUTTON_ATTACK;
		break;
	case BOTLITE_COMBO_KICK:
		cmd->buttons |= BUTTON_ALT_ATTACK;
		break;
	case BOTLITE_COMBO_SPEED:
		/* forward only: PM_Melee treats this as start / speed melee pressure */
		break;
	case BOTLITE_COMBO_FINISH:
		cmd->buttons |= BUTTON_ALT_ATTACK;
		break;
	}
}


void BotLite_RunCombatSkill1( gentity_t *bot, int clientNum, gentity_t *target, float distSq ) {
	botlite_info_t *info;
	usercmd_t *cmd;
	float dist;
	qboolean inMelee;

	info = &g_botlite[clientNum];
	cmd = &bot->client->pers.cmd;
	dist = (float)sqrt( distSq );
	inMelee = ( bot->client->ps.bitFlags & usingMelee ) ? qtrue : qfalse;

	BotLite_SetLockOn( bot, target );
	BotLite_FaceTarget( bot, target );
	BotLite_DebugLog( bot, "Combat tracking target" );

	/* Chase hard until PM_Melee actually latches, and keep flyboost active in melee. */
	cmd->forwardmove = 127;
	cmd->buttons |= BUTTON_BOOST;

	if ( dist > BOTLITE_MELEE_CHASE_DISTANCE && !inMelee ) {
		info->nextActionTime = 0;
		info->actionUntil = 0;
		return;
	}

	/* Inside real melee range, keep pressing forward until the engine puts us into usingMelee. */
	if ( !inMelee ) {
		if ( dist <= BOTLITE_MELEE_START_DISTANCE ) {
			info->nextActionTime = 0;
			info->actionUntil = 0;
		}
		return;
	}

	if ( inMelee ) {
		cmd->buttons |= BUTTON_BOOST;
	}

	/* When frozen, maintain lock and let the engine recover. */
	if ( bot->client->ps.timers[tmFreeze] > 0 || bot->client->ps.timers[tmMeleeIdle] < 0 ) {
		return;
	}

	if ( info->nextActionTime == 0 && info->actionUntil == 0 ) {
		BotLite_StartCombo( clientNum );
	}

	if ( info->actionUntil > 0 ) {
		if ( level.time < info->actionUntil ) {
			BotLite_ApplyComboButtons( info, cmd );
			return;
		}

		info->actionUntil = 0;
		BotLite_AdvanceCombo( clientNum );
	}

	if ( level.time < info->nextActionTime ) {
		return;
	}

	/* Start current combo input and keep it held for the proper window. */
	if ( info->comboStage == BOTLITE_COMBO_PUNCH ) {
		cmd->buttons |= BUTTON_ATTACK;
		info->actionUntil = level.time + 90;
	}
	else if ( info->comboStage == BOTLITE_COMBO_KICK ) {
		cmd->buttons |= BUTTON_ALT_ATTACK;
		info->actionUntil = level.time + 90;
	}
	else if ( info->comboStage == BOTLITE_COMBO_SPEED ) {
		info->actionUntil = level.time + 240;
	}
	else {
		cmd->buttons |= BUTTON_ALT_ATTACK;
		info->actionUntil = level.time + 650;
	}
}


void BotLite_RunCombatDefault( gentity_t *bot, gentity_t *target ) {
	BotLite_SetLockOn( bot, target );
	BotLite_FaceTarget( bot, target );
}


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


static int BotLite_PickSkill2Weapon( gentity_t *bot, int currentWeapon ) {
	int i;
	int count;
	int choices[MAX_PLAYERWEAPONS];
	int mask;

	if ( !bot || !bot->client ) {
		return 1;
	}

	mask = bot->client->ps.stats[stSkills];
	count = 0;
	for ( i = 1; i <= MAX_PLAYERWEAPONS; i++ ) {
		if ( mask & ( 1 << i ) ) {
			if ( BotLite_Skill2BotDisallowsWeapon( bot, i ) ) {
				continue;
			}
			choices[count++] = i;
		}
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
	return ( info->skill2AttackMode == 2 ) ? qtrue : qfalse;
}


static void BotLite_Skill2PressAttack( usercmd_t *cmd, qboolean useAlt ) {
	if ( useAlt ) {
		cmd->buttons |= BUTTON_ALT_ATTACK;
	} else {
		cmd->buttons |= BUTTON_ATTACK;
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

	info = &g_botlite[clientNum];
	info->skill2Weapon = BotLite_PickSkill2Weapon( bot, info->skill2Weapon + 1 );
	info->skill2WeaponSwitchTime = level.time + BOTLITE_SKILL2_WEAPON_SWITCH_TIME;
	info->skill2HoldUntil = 0;
	hasAlt = BotLite_Skill2WeaponHasAlt( clientNum, info->skill2Weapon );
	if ( info->skill2Weapon == 1 && hasAlt ) {
		info->skill2AttackMode = 2;
	} else {
		info->skill2AttackMode = hasAlt && ( rand() & 1 ) ? 2 : 1;
	}
}


static void BotLite_RunSkill2RangedPressure( gentity_t *bot, int clientNum, gentity_t *target ) {
	botlite_info_t *info;
	usercmd_t *cmd;
	int weapon;
	qboolean useAlt;
	qboolean charging;

	info = &g_botlite[clientNum];
	cmd = &bot->client->pers.cmd;

	if ( target ) {
		BotLite_SetLockOn( bot, target );
		BotLite_FaceTarget( bot, target );
	}

	if ( info->skill2Weapon <= 0 || info->skill2Weapon > MAX_PLAYERWEAPONS || level.time >= info->skill2WeaponSwitchTime ) {
		BotLite_StartSkill2Attack( bot, clientNum );
	}

	weapon = info->skill2Weapon;
	useAlt = BotLite_Skill2UseAltAttack( info );
	cmd->weapon = weapon;

	if ( bot->client->ps.weapon != weapon ) {
		return;
	}

	charging = ( useAlt && bot->client->ps.weaponstate == WEAPON_ALTCHARGING ) ||
		( !useAlt && bot->client->ps.weaponstate == WEAPON_CHARGING );

	if ( charging ) {
		if ( BotLite_Skill2ReadyToRelease( bot, useAlt ) ) {
			info->nextActionTime = level.time + 350;
			return;
		}

		BotLite_Skill2PressAttack( cmd, useAlt );
		return;
	}

	if ( bot->client->ps.weaponstate == WEAPON_READY ) {
		if ( level.time < info->nextActionTime ) {
			return;
		}

		BotLite_Skill2PressAttack( cmd, useAlt );
		return;
	}

	if ( BotLite_Skill2NeedsCharge( bot, useAlt ) ) {
		return;
	}

	if ( bot->client->ps.weaponstate == WEAPON_FIRING || bot->client->ps.weaponstate == WEAPON_GUIDING ||
		( useAlt && ( bot->client->ps.weaponstate == WEAPON_ALTFIRING || bot->client->ps.weaponstate == WEAPON_ALTGUIDING ) ) ) {
		BotLite_Skill2PressAttack( cmd, useAlt );
	}
}


void BotLite_RunSearchSkill2( gentity_t *bot, int clientNum, gentity_t *target, float distSq ) {
	botlite_info_t *info;
	usercmd_t *cmd;
	float dist;

	info = &g_botlite[clientNum];
	cmd = &bot->client->pers.cmd;
	if ( !target ) {
		info->lastTargetNum = -1;
		info->nextActionTime = 0;
		return;
	}

	dist = (float)sqrt( distSq );
	if ( !info->skill2DidSpawnFlyup ) {
		if ( info->nextActionTime <= 0 ) {
			info->nextActionTime = level.time + 2000;
			BotLite_DebugLog( bot, va( "Skill2 spawn flyup target=%d dist=%.0f until=%d", target->s.number, dist, info->nextActionTime ) );
		}

		info->lastTargetNum = target->s.number;

		if ( level.time < info->nextActionTime ) {
			cmd->upmove = 127;
			return;
		}

		info->skill2DidSpawnFlyup = qtrue;
		info->nextActionTime = 0;
	}

	info->lastTargetNum = target->s.number;
	info->mode = BOTLITE_MODE_COMBAT;
	info->nextActionTime = 0;
	BotLite_DebugLog( bot, va( "Skill2 search lock target=%d dist=%.0f", target->s.number, dist ) );
	if ( dist <= BOTLITE_SKILL2_RANGED_DISTANCE ) {
		info->skill2ForceMelee = qtrue;
		BotLite_DebugLog( bot, va( "Skill2 enter melee target=%d dist=%.0f", target->s.number, dist ) );
		BotLite_RunCombatSkill1( bot, clientNum, target, distSq );
		return;
	}

	info->skill2ForceMelee = qfalse;
	BotLite_RunSkill2RangedPressure( bot, clientNum, target );
}


void BotLite_RunCombatSkill2( gentity_t *bot, int clientNum, gentity_t *target, float distSq ) {
	botlite_info_t *info;
	float dist;
	qboolean inMelee;

	info = &g_botlite[clientNum];
	dist = (float)sqrt( distSq );
	inMelee = ( bot->client->ps.bitFlags & usingMelee ) ? qtrue : qfalse;

	BotLite_SetLockOn( bot, target );
	BotLite_FaceTarget( bot, target );

	if ( inMelee ) {
		info->skill2ForceMelee = qtrue;
	}

	if ( !info->skill2ForceMelee && dist <= BOTLITE_SKILL2_RANGED_DISTANCE ) {
		info->skill2ForceMelee = qtrue;
		BotLite_DebugLog( bot, va( "Skill2 enter melee target=%d dist=%.0f", target ? target->s.number : -1, dist ) );
	}

	if ( info->skill2ForceMelee ) {
		if ( dist > BOTLITE_SKILL2_MELEE_EXIT_DISTANCE && !inMelee ) {
			info->skill2ForceMelee = qfalse;
			BotLite_StartSkill2Attack( bot, clientNum );
			BotLite_DebugLog( bot, va( "Skill2 exit melee to ranged target=%d dist=%.0f", target ? target->s.number : -1, dist ) );
		} else {
			BotLite_RunCombatSkill1( bot, clientNum, target, distSq );
			return;
		}
	}

	BotLite_RunSkill2RangedPressure( bot, clientNum, target );
}


