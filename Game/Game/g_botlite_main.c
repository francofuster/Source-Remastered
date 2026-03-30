#include "g_local.h"
#include "g_botlite.h"

static qboolean BotLite_IsTargetDead( gentity_t *target );
static void BotLite_ResetBotStateForTargetDeath( int clientNum );
static void BotLite_ResetBotStateForRespawn( int clientNum );

botlite_info_t g_botlite[BOTLITE_MAX_BOTS];

static qboolean BotLite_IsManagedBot( int clientNum );
static int BotLite_ClampSkill( int skill );

void BotLite_ResetAll( void ) {
	memset( g_botlite, 0, sizeof( g_botlite ) );
}


void BotLite_OnClientDisconnect( int clientNum ) {
	if ( clientNum < 0 || clientNum >= level.maxclients ) {
		return;
	}
	memset( &g_botlite[clientNum], 0, sizeof( g_botlite[clientNum] ) );
	g_botlite[clientNum].lastLoggedMode = -1;
}


static qboolean BotLite_IsTargetDead( gentity_t *target ) {
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

static void BotLite_ResetBotStateForTargetDeath( int clientNum ) {
	botlite_info_t *info;

	info = &g_botlite[clientNum];

	info->mode = ( info->skill == 1 || info->skill == 2 ) ? BOTLITE_MODE_SEARCH : BOTLITE_MODE_IDLE;
	info->didInitialRise = qtrue; /* important: reset everything but skip flyup */
	info->riseEndTime = 0;
	info->turnEndTime = 0;
	info->moveEndTime = 0;
	info->turnYawStart = 0.0f;
	info->moveYaw = 0.0f;
	info->lastTargetNum = -1;
	info->comboStage = 0;
	info->comboStep = 0;
	info->actionUntil = 0;
	info->nextActionTime = 0;
	info->lastDamageEvent = 0;
	info->crashStartTime = 0;
	info->skill2AttackMode = 0;
	info->skill2WeaponSwitchTime = 0;
	info->skill2HoldUntil = 0;
	info->skill2ApproachTime = 0;
	info->skill2ForceMelee = qfalse;
	info->retreatUntil = 0;
	info->recoveryWaitEndTime = 0;
	info->recoveryWaitCooldownUntil = 0;
	info->retreatYaw = 0.0f;
	info->postCrashFlyup = qfalse;
	info->postCrashRiseEndTime = 0;
	info->targetRecoveryHandled = qfalse;
	info->targetRecoveryHandledNum = -1;
	info->targetRecoveryHandledEvent = -1;
	info->crashDiagNextLogTime = 0;
	info->targetCrashActive = qfalse;
	info->skill2DidSpawnFlyup = qtrue;

	BotLite_ClearLock( &g_entities[clientNum] );
}


static void BotLite_ResetBotStateForRespawn( int clientNum ) {
	botlite_info_t *info;

	info = &g_botlite[clientNum];

	info->mode = ( info->skill == 1 || info->skill == 2 ) ? BOTLITE_MODE_SEARCH : BOTLITE_MODE_IDLE;
	info->didInitialRise = qfalse; /* respawn should behave like the initial spawn */
	info->riseEndTime = 0;
	info->turnEndTime = 0;
	info->moveEndTime = 0;
	info->turnYawStart = 0.0f;
	info->moveYaw = 0.0f;
	info->lastTargetNum = -1;
	info->comboStage = 0;
	info->comboStep = 0;
	info->actionUntil = 0;
	info->nextActionTime = 0;
	info->lastDamageEvent = 0;
	info->crashStartTime = 0;
	info->skill2AttackMode = 0;
	info->skill2WeaponSwitchTime = 0;
	info->skill2HoldUntil = 0;
	info->skill2ApproachTime = 0;
	info->skill2ForceMelee = qfalse;
	info->retreatUntil = 0;
	info->recoveryWaitEndTime = 0;
	info->recoveryWaitCooldownUntil = 0;
	info->retreatYaw = 0.0f;
	info->postCrashFlyup = qfalse;
	info->postCrashRiseEndTime = 0;
	info->targetRecoveryHandled = qfalse;
	info->targetRecoveryHandledNum = -1;
	info->targetRecoveryHandledEvent = -1;
	info->crashDiagNextLogTime = 0;
	info->targetCrashActive = qfalse;
	info->skill2DidSpawnFlyup = qfalse;

	/* Re-roll or reselect runtime-only behavior just like a new spawn. */
	info->skill2Weapon = 0;

	BotLite_ClearLock( &g_entities[clientNum] );
}

static qboolean BotLite_IsManagedBot( int clientNum ) {
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


int BotLite_AddBot( const char *characterName, int skill, char *error, int errorSize ) {
	int clientNum;
	fileHandle_t f;
	char userinfo[MAX_INFO_STRING];
	const char *modelName;
	const char *teamName;
	char characterClean[MAX_QPATH];
	char physPath[MAX_QPATH];
	botlite_info_t *info;

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
	modelName = characterClean;

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
	Info_SetValueForKey( userinfo, "name", modelName );
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
	Info_SetValueForKey( userinfo, "skill", va( "%d", BotLite_ClampSkill( skill ) ) );

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
	info = &g_botlite[clientNum];
	memset( info, 0, sizeof( *info ) );
	info->lastLoggedMode = -1;
	info->inuse = qtrue;
	info->skill = BotLite_ClampSkill( skill );
	info->mode = ( info->skill == 1 || info->skill == 2 ) ? BOTLITE_MODE_SEARCH : BOTLITE_MODE_IDLE;
	info->lastTargetNum = -1;
	info->skill2Weapon = 0;
	info->skill2AttackMode = 0;
	info->skill2WeaponSwitchTime = 0;
	info->skill2HoldUntil = 0;
	info->skill2ApproachTime = 0;
	info->retreatUntil = 0;
	info->recoveryWaitEndTime = 0;
	info->recoveryWaitCooldownUntil = 0;
	info->retreatYaw = 0.0f;
	info->postCrashFlyup = qfalse;
	info->postCrashRiseEndTime = 0;
	info->targetRecoveryHandled = qfalse;
	info->targetRecoveryHandledNum = -1;
	info->targetRecoveryHandledEvent = -1;
	info->crashDiagNextLogTime = 0;
	info->targetCrashActive = qfalse;
			info->skill2ForceMelee = qfalse;
	info->skill2ForceMelee = qfalse;
	Q_strncpyz( info->character, modelName, sizeof( info->character ) );

	return clientNum;
}


int BotLite_AddBotDebug( const char *characterName, int skill, char *error, int errorSize ) {
	int clientNum;

	clientNum = BotLite_AddBot( characterName, skill, error, errorSize );
	if ( clientNum >= 0 ) {
		g_botlite[clientNum].debugEnabled = qtrue;
		g_botlite[clientNum].lastLoggedMode = -1;
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
			continue;
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


int BotAISetup( int restart ) {
	return 0;
}


int BotAIShutdown( int restart ) {
	return 0;
}


int BotAILoadMap( int restart ) {
	return 0;
}


int BotAISetupClient( int client, struct bot_settings_s *settings, qboolean restart ) {
	return 0;
}


int BotAIShutdownClient( int client, qboolean restart ) {
	BotLite_OnClientDisconnect( client );
	return 0;
}


int BotAIStartFrame( int time ) {
	int i;

	for ( i = 0; i < level.maxclients; i++ ) {
		gentity_t *ent;
		gentity_t *target;
		usercmd_t *cmd;
		float distSq;
		botlite_info_t *info;
		qboolean targetCrashEdge;
		qboolean targetCrashNow;
		qboolean targetCrashPartial;

		if ( !BotLite_IsManagedBot( i ) ) {
			continue;
		}

		ent = &g_entities[i];
		if ( !ent->inuse || !ent->client || !( ent->r.svFlags & SVF_BOT ) ) {
			continue;
		}
		if ( ent->client->pers.connected != CON_CONNECTED ) {
			continue;
		}
		if ( ent->client->sess.sessionTeam == TEAM_SPECTATOR ) {
			continue;
		}

		cmd = &ent->client->pers.cmd;
		memset( cmd, 0, sizeof( *cmd ) );
		cmd->serverTime = time;

		if ( ent->client->ps.bitFlags & isDead ) {
			BotLite_DebugLog( ent, "Bot died -> full respawn reset" );
			BotLite_ResetBotStateForRespawn( i );
			continue;
		}

		info = &g_botlite[i];
		if ( BotLite_IsTemporarilyDisabled( ent ) ) {
			qboolean realCrash;
			realCrash = ( ent->client->ps.bitFlags & isCrashed ) ||
				( ent->client->ps.bitFlags & isUnconcious ) ||
				ent->client->ps.timers[tmCrash] != 0 ||
				ent->client->ps.timers[tmRecover] != 0 ||
				ent->client->ps.powerups[PW_STATE] == -1;
			if ( realCrash ) {
				if ( !info->postCrashFlyup ) {
					BotLite_DebugLog( ent, va( "Real crash detected on bot: dmgEv=%d tmCrash=%d tmRecover=%d", ent->client->ps.damageEvent, ent->client->ps.timers[tmCrash], ent->client->ps.timers[tmRecover] ) );
				}
				info->didInitialRise = qfalse;
				info->postCrashFlyup = qtrue;
			}
			cmd->forwardmove = 0;
			cmd->rightmove = 0;
			cmd->upmove = 0;
			cmd->buttons = 0;
			info->nextActionTime = 0;
			info->actionUntil = 0;
			info->comboStep = 0;
			info->skill2HoldUntil = 0;
			continue;
		}
		if ( info->lastLoggedMode != info->mode ) {
			BotLite_DebugLog( ent, "Mode changed" );
			info->lastLoggedMode = info->mode;
		}
		if ( BotLite_RunPostCrashFlyup( ent, i ) ) {
			continue;
		}
		target = NULL;
		distSq = BOTLITE_LOCK_RANGE * BOTLITE_LOCK_RANGE;
		if ( info->mode == BOTLITE_MODE_WAIT_TARGET_RECOVERY ) {
			target = BotLite_GetTrackedTarget( ent, i, &distSq );
			if ( BotLite_RunRecoveryWait( ent, i, target ) ) {
				continue;
			}
			target = NULL;
			distSq = BOTLITE_LOCK_RANGE * BOTLITE_LOCK_RANGE;
		}

		if ( info->skill == 1 && BotLite_ReactToDamage( ent, i, &distSq ) ) {
			target = BotLite_GetTrackedTarget( ent, i, &distSq );
		}

		if ( !target ) {
			if ( info->skill == 2 ) {
				target = BotLite_FindNearestPlayerAnyDistance( ent, &distSq );
				if ( target ) {
					info->lastTargetNum = target->s.number;
				}
			}
			else if ( info->mode == BOTLITE_MODE_COMBAT ) {
				target = BotLite_GetTrackedTarget( ent, i, &distSq );
			} else {
				target = BotLite_FindNearestVisiblePlayer( ent, &distSq );
				if ( target ) {
					info->lastTargetNum = target->s.number;
				}
			}
		}
		targetCrashEdge = qfalse;
		targetCrashNow = qfalse;
		targetCrashPartial = qfalse;
		if ( target && target->client ) {
			if ( info->targetRecoveryHandledNum != target->s.number ) {
				info->targetRecoveryHandled = qfalse;
				info->targetRecoveryHandledNum = target->s.number;
				info->targetRecoveryHandledEvent = target->client->botCrashEventCounter;
				info->targetCrashActive = qfalse;
				info->crashDiagNextLogTime = 0;
				BotLite_DebugLog( ent, va( "Target switch sync target=%d crashCounter=%d", target->s.number, target->client->botCrashEventCounter ) );
			}
			targetCrashNow = BotLite_TargetNeedsRecoveryWait( target );
			targetCrashPartial = ( target->client->ps.timers[tmKnockback] > 0 ) ||
				( target->client->ps.timers[tmCrash] > 0 ) ||
				( target->client->ps.timers[tmRecover] > 0 ) ||
				( target->client->ps.powerups[PW_STATE] == -1 ) ||
				( target->client->ps.bitFlags & isCrashed ) ||
				( target->client->ps.bitFlags & isUnconcious );

			if ( targetCrashNow && !info->targetCrashActive ) {
				targetCrashEdge = qtrue;
				BotLite_DebugLog( ent, va( "Target crash state edge target=%d crashCounter=%d handledCounter=%d flags=%d tmK=%d tmC=%d tmR=%d pw=%d",
					target->s.number,
					target->client->botCrashEventCounter,
					info->targetRecoveryHandledEvent,
					target->client->ps.bitFlags,
					target->client->ps.timers[tmKnockback],
					target->client->ps.timers[tmCrash],
					target->client->ps.timers[tmRecover],
					target->client->ps.powerups[PW_STATE] ) );
			} else if ( target->client->botCrashEventCounter != info->targetRecoveryHandledEvent ) {
				targetCrashEdge = qtrue;
				BotLite_DebugLog( ent, va( "Target crash counter edge target=%d crashCounter=%d handledCounter=%d flags=%d tmK=%d tmC=%d tmR=%d pw=%d",
					target->s.number,
					target->client->botCrashEventCounter,
					info->targetRecoveryHandledEvent,
					target->client->ps.bitFlags,
					target->client->ps.timers[tmKnockback],
					target->client->ps.timers[tmCrash],
					target->client->ps.timers[tmRecover],
					target->client->ps.powerups[PW_STATE] ) );
			} else if ( targetCrashNow && target->client->botCrashEventCounter == info->targetRecoveryHandledEvent ) {
				if ( level.time >= info->crashDiagNextLogTime ) {
					BotLite_DebugLog( ent, va( "Crash still active after edge target=%d crashCounter=%d handledCounter=%d stateSeen=%d",
						target->s.number,
						target->client->botCrashEventCounter,
						info->targetRecoveryHandledEvent,
						info->targetCrashActive ) );
					info->crashDiagNextLogTime = level.time + 1000;
				}
			} else if ( !targetCrashNow && targetCrashPartial ) {
				if ( level.time >= info->crashDiagNextLogTime ) {
					BotLite_DebugLog( ent, va( "Partial crash/knockback state only: target=%d crashCounter=%d flags=%d tmK=%d tmC=%d tmR=%d pw=%d",
						target->s.number,
						target->client->botCrashEventCounter,
						target->client->ps.bitFlags,
						target->client->ps.timers[tmKnockback],
						target->client->ps.timers[tmCrash],
						target->client->ps.timers[tmRecover],
						target->client->ps.powerups[PW_STATE] ) );
					info->crashDiagNextLogTime = level.time + 1000;
				}
			} else if ( !targetCrashNow ) {
				if ( level.time >= info->crashDiagNextLogTime ) {
					BotLite_DebugLog( ent, va( "No crash on target=%d crashCounter=%d flags=%d tmK=%d tmC=%d tmR=%d pw=%d distSq=%.0f",
						target->s.number,
						target->client->botCrashEventCounter,
						target->client->ps.bitFlags,
						target->client->ps.timers[tmKnockback],
						target->client->ps.timers[tmCrash],
						target->client->ps.timers[tmRecover],
						target->client->ps.powerups[PW_STATE],
						distSq ) );
					info->crashDiagNextLogTime = level.time + 1500;
				}
			}

			info->targetCrashActive = targetCrashNow;
		} else if ( info->mode != BOTLITE_MODE_WAIT_TARGET_RECOVERY ) {
			info->targetRecoveryHandled = qfalse;
			info->targetRecoveryHandledNum = -1;
			info->targetRecoveryHandledEvent = -1;
			info->crashDiagNextLogTime = 0;
	info->targetCrashActive = qfalse;
		}


		if ( target && BotLite_IsTargetDead( target ) ) {
			BotLite_DebugLog( ent, va( "Target died -> reset bot state target=%d", target->s.number ) );
			BotLite_ResetBotStateForTargetDeath( i );
			continue;
		}

		if ( info->mode == BOTLITE_MODE_SEARCH ) {
			if ( info->skill == 1 ) {
				BotLite_RunSearchSkill1( ent, i, target );
			} else if ( info->skill == 2 ) {
				BotLite_RunSearchSkill2( ent, i, target, distSq );
			} else {
				BotLite_RunSearchDefault( ent, i, target );
			}
		}
		else if ( info->mode == BOTLITE_MODE_COMBAT ) {
			if ( !target ) {
				if ( info->lastTargetNum >= 0 ) {
					BotLite_DebugLog( ent, va( "Lost target during combat lastTarget=%d", info->lastTargetNum ) );
				}
				BotLite_ClearLock( ent );
				info->mode = ( info->skill == 1 || info->skill == 2 ) ? BOTLITE_MODE_SEARCH : BOTLITE_MODE_IDLE;
				info->lastTargetNum = -1;
				info->nextActionTime = 0;
				info->actionUntil = 0;
				continue;
			}
			if ( targetCrashEdge ) {
				info->targetRecoveryHandled = qtrue;
				info->targetRecoveryHandledNum = target->s.number;
				info->targetRecoveryHandledEvent = target->client->botCrashEventCounter;
				BotLite_DebugLog( ent, va( "Trigger retreat target=%d crashCounter=%d", target->s.number, target->client->botCrashEventCounter ) );
				BotLite_ClearLock( ent );
				BotLite_StartRecoveryWait( ent, i, target );
				if ( BotLite_RunRecoveryWait( ent, i, target ) ) {
					continue;
				}
			}
			if ( info->skill == 1 ) {
				BotLite_RunCombatSkill1( ent, i, target, distSq );
			} else if ( info->skill == 2 ) {
				BotLite_RunCombatSkill2( ent, i, target, distSq );
			} else {
				BotLite_RunCombatDefault( ent, target );
			}
		}
		else if ( target ) {
			if ( info->skill == 2 ) {
				info->mode = BOTLITE_MODE_COMBAT;
				BotLite_RunCombatSkill2( ent, i, target, distSq );
			} else {
				BotLite_FaceTarget( ent, target );
			}
		}
	}

	return 0;
}


