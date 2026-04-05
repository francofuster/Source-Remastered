#include "g_local.h"
#include "g_botlite.h"

botlite_info_t g_botlite[BOTLITE_MAX_BOTS];

static int BotLite_ClampSkill( int skill );
static void BotLite_InitializeBotInfo( int clientNum, const char *displayName, const char *modelName, const char *archetypeName, int skill );

void BotLite_ResetAll( void ) {
	memset( g_botlite, 0, sizeof( g_botlite ) );
	BotLite_BotsysInit();
}

void BotLite_OnClientDisconnect( int clientNum ) {
	if ( clientNum < 0 || clientNum >= level.maxclients ) {
		return;
	}
	memset( &g_botlite[clientNum], 0, sizeof( g_botlite[clientNum] ) );
	g_botlite[clientNum].runtime.lastLoggedMode = -1;
	g_botlite[clientNum].runtime.lastGoal = BOTLITE_GOAL_NONE;
	g_botlite[clientNum].runtime.lastTactic = BOTLITE_TACTIC_NONE;
	g_botlite[clientNum].runtime.plannedTactic = BOTLITE_TACTIC_NONE;
	g_botlite[clientNum].runtime.nextTacticThinkTime = 0;
}

void BotLite_ResetTransientCombatState( int clientNum ) {
	botlite_info_t *info;

	info = &g_botlite[clientNum];
	info->melee.nextActionTime = 0;
	info->melee.actionUntil = 0;
	info->melee.comboStep = 0;
	info->melee.comboTargetHits = 0;
	info->melee.blockUntil = 0;
	info->melee.nextBlockTime = 0;
	info->melee.dodgeUntil = 0;
	info->melee.nextDodgeTime = 0;
	info->melee.dodgeDirection = 1;
	info->melee.specialHoldUntil = 0;
	info->melee.knockbackDirection = BOTLITE_SKILL3_KB_FORWARD;
	info->melee.combatState = BOTLITE_COMBAT_STATE_NONE;
	info->melee.combatNextThinkTime = 0;
	info->melee.combatStateChangedTime = 0;
	info->ranged.holdUntil = 0;
	info->runtime.plannedTactic = BOTLITE_TACTIC_NONE;
	info->runtime.nextTacticThinkTime = 0;
}

void BotLite_ResetCoreRuntimeState( int clientNum, qboolean respawnStyle, qboolean clearTarget ) {
	botlite_info_t *info;

	info = &g_botlite[clientNum];
	info->runtime.mode = BotLite_DefaultModeForSkill( info->skill );
	info->search.didInitialRise = respawnStyle ? qfalse : qtrue;
	info->search.riseEndTime = 0;
	info->search.turnEndTime = 0;
	info->search.moveEndTime = 0;
	info->search.turnYawStart = 0.0f;
	info->search.moveYaw = 0.0f;
	if ( clearTarget ) {
		info->runtime.lastTargetNum = -1;
	}
	info->melee.comboStage = 0;
	info->melee.comboStep = 0;
	info->melee.comboTargetHits = 0;
	info->melee.actionUntil = 0;
	info->melee.nextActionTime = 0;
	info->melee.blockUntil = 0;
	info->melee.nextBlockTime = 0;
	info->melee.dodgeUntil = 0;
	info->melee.nextDodgeTime = 0;
	info->melee.dodgeDirection = 1;
	info->melee.specialHoldUntil = 0;
	info->melee.knockbackDirection = BOTLITE_SKILL3_KB_FORWARD;
	info->melee.combatState = BOTLITE_COMBAT_STATE_NONE;
	info->melee.combatNextThinkTime = 0;
	info->melee.combatStateChangedTime = 0;
	info->runtime.lastDamageEvent = 0;
	info->runtime.crashStartTime = 0;
	info->ranged.attackMode = 0;
	info->ranged.weaponSwitchTime = 0;
	info->ranged.holdUntil = 0;
	info->ranged.approachTime = 0;
	info->ranged.forceMelee = qfalse;
	info->ranged.didInitialTransform = respawnStyle ? qfalse : info->ranged.didInitialTransform;
	info->ranged.openingStyle = BOTLITE_SKILL3_OPENING_NONE;
	info->ranged.openingTargetNum = -1;
	info->recovery.retreatUntil = 0;
	info->recovery.recoveryWaitEndTime = 0;
	info->recovery.recoveryWaitCooldownUntil = 0;
	info->recovery.retreatYaw = 0.0f;
	info->recovery.postCrashFlyup = qfalse;
	info->recovery.postCrashRiseEndTime = 0;
	info->recovery.healRequested = qfalse;
	info->recovery.healActive = qfalse;
	info->recovery.healLastHealth = 0;
	info->recovery.healLastCurrent = 0;
	info->recovery.healLastProgressTime = 0;
	info->recovery.knockbackStartTime = 0;
	info->runtime.targetRecoveryHandled = qfalse;
	info->runtime.targetRecoveryHandledNum = -1;
	info->runtime.targetRecoveryHandledEvent = -1;
	info->runtime.crashDiagNextLogTime = 0;
	info->runtime.targetCrashActive = qfalse;
	info->runtime.conserveBoost = qfalse;
	info->runtime.lastGoal = BOTLITE_GOAL_NONE;
	info->runtime.lastTactic = BOTLITE_TACTIC_NONE;
	info->runtime.plannedTactic = BOTLITE_TACTIC_NONE;
	info->runtime.nextTacticThinkTime = 0;
	info->ranged.didSpawnFlyup = respawnStyle ? qfalse : qtrue;
	if ( respawnStyle ) {
		info->ranged.weapon = 0;
		info->ranged.didInitialTransform = qfalse;
	}
	if ( info->skillOps && info->skillOps->ResetRuntime ) {
		info->skillOps->ResetRuntime( clientNum, respawnStyle );
	}

	memset( &info->action, 0, sizeof( info->action ) );
	BotLite_ClearLock( &g_entities[clientNum] );
}

void BotLite_ResetStateForTargetDeath( int clientNum ) {
	BotLite_ResetCoreRuntimeState( clientNum, qfalse, qtrue );
}

void BotLite_ResetStateForRespawn( int clientNum ) {
	BotLite_ResetCoreRuntimeState( clientNum, qtrue, qtrue );
}

void BotLite_ResetStateForLostTarget( int clientNum ) {
	BotLite_ResetCoreRuntimeState( clientNum, qfalse, qtrue );
}

qboolean BotLite_IsManagedBot( int clientNum ) {
	if ( clientNum < 0 || clientNum >= level.maxclients ) {
		return qfalse;
	}
	return g_botlite[clientNum].inuse;
}

static int BotLite_ClampSkill( int skill ) {
	if ( skill < 1 ) {
		return 1;
	}
	if ( skill > 3 ) {
		return 3;
	}
	return skill;
}

static void BotLite_InitializeBotInfo( int clientNum, const char *displayName, const char *modelName, const char *archetypeName, int skill ) {
	botlite_info_t *info;

	info = &g_botlite[clientNum];
	memset( info, 0, sizeof( *info ) );
	info->runtime.lastLoggedMode = -1;
	info->runtime.lastGoal = BOTLITE_GOAL_NONE;
	info->runtime.lastTactic = BOTLITE_TACTIC_NONE;
	info->runtime.plannedTactic = BOTLITE_TACTIC_NONE;
	info->runtime.nextTacticThinkTime = 0;
	info->inuse = qtrue;
	info->skill = BotLite_ClampSkill( skill );
	info->profile = BotLite_GetProfile( info->skill );
	info->skillOps = BotLite_GetSkillVTable( info->skill );
	info->runtime.mode = BotLite_DefaultModeForSkill( info->skill );
	info->runtime.lastTargetNum = -1;
	info->runtime.targetRecoveryHandledNum = -1;
	info->runtime.targetRecoveryHandledEvent = -1;
	Q_strncpyz( info->character, displayName && displayName[0] ? displayName : modelName, sizeof( info->character ) );
	Q_strncpyz( info->modelName, modelName ? modelName : info->character, sizeof( info->modelName ) );
	BotLite_BotsysApplyInfoConfig( info, archetypeName );
}

int BotLite_AddBot( const char *characterName, int skill, char *error, int errorSize ) {
	int clientNum;
	fileHandle_t f;
	char userinfo[MAX_INFO_STRING];
	const char *modelName;
	const char *teamName;
	char characterClean[MAX_QPATH];
	char displayName[MAX_QPATH];
	char resolvedModelName[MAX_QPATH];
	char archetypeName[MAX_QPATH];
	char physPath[MAX_QPATH];
	int resolvedSkill;

	if ( error && errorSize > 0 ) {
		error[0] = '\0';
	}

	if ( !characterName || !characterName[0] ) {
		if ( error ) {
			Q_strncpyz( error, "Usage: /addbot <character> [skill 1-3]", errorSize );
		}
		return -1;
	}

	Q_strncpyz( characterClean, characterName, sizeof( characterClean ) );
	BotLite_BotsysResolveBot( characterClean, skill, displayName, sizeof( displayName ), resolvedModelName, sizeof( resolvedModelName ), &resolvedSkill, archetypeName, sizeof( archetypeName ) );
	modelName = resolvedModelName[0] ? resolvedModelName : characterClean;
	if ( resolvedSkill <= 0 ) {
		resolvedSkill = skill > 0 ? skill : 1;
	}

	Com_sprintf( physPath, sizeof( physPath ), "players/%s/default.phys", modelName );
	if ( trap_FS_FOpenFile( physPath, &f, FS_READ ) <= 0 ) {
		if ( error ) {
			Com_sprintf( error, errorSize, "Character '%s' was not found.", modelName );
		}
		return -1;
	}
	trap_FS_FCloseFile( f );

	clientNum = trap_BotAllocateClient();
	if ( clientNum < 0 ) {
		if ( error ) {
			Q_strncpyz( error, "No free client slots available for a bot.", errorSize );
		}
		return -1;
	}

	memset( userinfo, 0, sizeof( userinfo ) );
	Info_SetValueForKey( userinfo, "name", displayName[0] ? displayName : modelName );
	Info_SetValueForKey( userinfo, "rate", "25000" );
	Info_SetValueForKey( userinfo, "snaps", "20" );
	Info_SetValueForKey( userinfo, "ip", "localhost" );
	Info_SetValueForKey( userinfo, "cg_predictItems", "0" );
	Info_SetValueForKey( userinfo, "cg_advancedFlight", "1" );
	Info_SetValueForKey( userinfo, "handicap", "100" );
	Info_SetValueForKey( userinfo, "model", modelName );
	Info_SetValueForKey( userinfo, "headmodel", modelName );
	Info_SetValueForKey( userinfo, "legsmodel", modelName );
	Info_SetValueForKey( userinfo, "color1", "4" );
	Info_SetValueForKey( userinfo, "color2", "4" );
	Info_SetValueForKey( userinfo, "skill", va( "%d", BotLite_ClampSkill( resolvedSkill ) ) );

	if ( g_gametype.integer >= GT_TEAM ) {
		team_t team;
		team = PickTeam( clientNum );
		teamName = ( team == TEAM_BLUE ) ? "blue" : "red";
		Info_SetValueForKey( userinfo, "team", teamName );
	}

	trap_SetUserinfo( clientNum, userinfo );
	g_entities[clientNum].r.svFlags |= SVF_BOT;

	if ( ClientConnect( clientNum, qtrue, qtrue ) ) {
		trap_BotFreeClient( clientNum );
		if ( error ) {
			Q_strncpyz( error, "ClientConnect rejected the bot.", errorSize );
		}
		return -1;
	}

	ClientBegin( clientNum );
	BotLite_InitializeBotInfo( clientNum, displayName, modelName, archetypeName, resolvedSkill );
	return clientNum;
}

int BotLite_AddBotDebug( const char *characterName, int skill, char *error, int errorSize ) {
	int clientNum;

	clientNum = BotLite_AddBot( characterName, skill, error, errorSize );
	if ( clientNum >= 0 ) {
		g_botlite[clientNum].runtime.debugEnabled = qtrue;
		g_botlite[clientNum].runtime.lastLoggedMode = -1;
		BotLite_DebugLog( &g_entities[clientNum], "Debug enabled" );
	}
	return clientNum;
}

int BotLite_RemoveBotByName( const char *characterName, char *error, int errorSize ) {
	int i;
	char cleanArg[MAX_QPATH];

	if ( error && errorSize > 0 ) {
		error[0] = '\0';
	}

	if ( !characterName || !characterName[0] ) {
		if ( error ) {
			Q_strncpyz( error, "Usage: /removebot <character>", errorSize );
		}
		return -1;
	}

	Q_strncpyz( cleanArg, characterName, sizeof( cleanArg ) );
	Q_CleanStr( cleanArg );

	for ( i = 0; i < level.maxclients; i++ ) {
		char cleanBotName[MAX_QPATH];
		gentity_t *ent;

		if ( !BotLite_IsManagedBot( i ) ) {
			continue;
		}

		ent = &g_entities[i];
		Q_strncpyz( cleanBotName, g_botlite[i].character, sizeof( cleanBotName ) );
		Q_CleanStr( cleanBotName );
		if ( Q_stricmp( cleanBotName, cleanArg ) != 0 ) {
			Q_strncpyz( cleanBotName, g_botlite[i].modelName, sizeof( cleanBotName ) );
			Q_CleanStr( cleanBotName );
			if ( Q_stricmp( cleanBotName, cleanArg ) != 0 ) {
				continue;
			}
		}

		BotLite_ClearLock( ent );
		trap_DropClient( i, "bot removed" );
		return i;
	}

	if ( error ) {
		Com_sprintf( error, errorSize, "Bot '%s' was not found.", characterName );
	}
	return -1;
}

int BotLite_RemoveAllBots( void ) {
	int i;
	int removed;

	removed = 0;
	for ( i = 0; i < level.maxclients; i++ ) {
		if ( !BotLite_IsManagedBot( i ) ) {
			continue;
		}
		BotLite_ClearLock( &g_entities[i] );
		trap_DropClient( i, "all bots removed" );
		removed++;
	}
	return removed;
}

int BotAISetup( int restart ) { return 0; }
int BotAIShutdown( int restart ) { return 0; }
int BotAILoadMap( int restart ) { return 0; }
int BotAISetupClient( int client, struct bot_settings_s *settings, qboolean restart ) { return 0; }

int BotAIShutdownClient( int client, qboolean restart ) {
	BotLite_OnClientDisconnect( client );
	return 0;
}

int BotAIStartFrame( int time ) {
	int i;

	for ( i = 0; i < level.maxclients; i++ ) {
		BotLite_ThinkClient( i, time );
	}

	return 0;
}
