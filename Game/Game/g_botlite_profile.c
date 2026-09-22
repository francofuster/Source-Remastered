#include "g_local.h"
#include "g_botlite.h"

#define BOTLITE_MAX_ARCHETYPES 32
#define BOTLITE_MAX_BOT_DEFS 64
#define BOTLITE_FILE_BUFFER_SIZE 32768

typedef struct {
	qboolean inuse;
	char sourceName[MAX_QPATH];
	char name[MAX_QPATH];
	float aggression;
	float rushTendency;
	float blockTendency;
	float specialTendency;
	float rangedBias;
	float comboCommitment;
	float targetSwitchBias;
	float powerlevelThreshold;
	float recoverRespect;
} botlite_archetype_cfg_t;

typedef struct {
	qboolean inuse;
	char id[MAX_QPATH];
	char displayName[MAX_QPATH];
	char playerModel[MAX_QPATH];
	char archetype[MAX_QPATH];
	int skill;
} botlite_botdef_t;

static const botlite_profile_t botlite_skill1_profile_default = {
	10000.0f,	/* acquireDistance */
	64.0f,		/* combatDistance */
	10000.0f,	/* meleeStartDistance */
	10000.0f,	/* meleeChaseDistance */
	2200.0f,	/* lockRange */
	3000,		/* searchRiseTime */
	450,		/* searchTurnTime */
	3000,		/* searchForwardTime */
	35.0f,		/* searchTurnMin */
	120.0f,	/* searchTurnMax */
	1000000.0f,	/* skill2AcquireDistance */
	10000.0f,	/* skill2RangedDistance */
	10000.0f,	/* skill2MeleeExitDistance */
	1500.0f,	/* skill2ApproachTriggerDistance */
	5000,		/* skill2ApproachInterval */
	1200,		/* skill2ApproachTime */
	5000,		/* skill2WeaponSwitchTime */
	25,		/* skill2ChargeMin */
	3000,		/* recoveryRetreatTime */
	1000,		/* postCrashFlyupTime */
	2000,		/* searchFlyupTime */
	0.50f,		/* skill3HealthStartPct */
	0.75f,		/* skill3HealthStopPct */
	900,		/* skill3BlockMinInterval */
	1800,		/* skill3BlockMaxInterval */
	260,		/* skill3BlockMinDuration */
	520,		/* skill3BlockMaxDuration */
	3000,		/* skill3DodgeMinInterval */
	3000,		/* skill3DodgeMaxInterval */
	1000,		/* skill3DodgeDuration */
	650,		/* skill3PowerMeleeChargeTime */
	2000,		/* skill3KnockbackCancelDelay */
	0.80f,		/* rangedChargeReleasePct */
	12000.0f,	/* skill3HealRetreatMaxDistance */
	0.0f,		/* targetAcquireMinDistance */
	qfalse,		/* targetAcquireRequiresLOS */
	0.0f,		/* rangedToMeleeDistance */
	0.0f,		/* meleeToRangedDistance */
	220,		/* reactiveBlockMinReaction */
	520,		/* reactiveBlockMaxReaction */
	70,		/* staminaComfortablePct */
	30,		/* staminaTightPct */
	10,		/* staminaCriticalPct */
	500.0f,	/* zanzokenEscapeDist */
	1200.0f,	/* kiChargeMinSafeDist */
	60,		/* kiChargeTargetPct */
	28,		/* recoverEnterPct */
	62,		/* recoverExitPct */
	40,		/* recoverHealthEnterPct */
	70,		/* recoverHealthExitPct */
	1500.0f,	/* sanzokenBandMinDist */
	16000.0f,	/* sanzokenBandMaxDist */
	12000,	/* recoverMaxMs */
	900.0f,	/* recoverSafeDist */
	22,		/* recoverKiFloorPct */
	4,		/* recoverKiFloorHiddenPct */
	85,		/* breakLimitMinKiPct */
	550,		/* breakLimitHoldMs */
	75,		/* struggleCommitPct */
	130,		/* breakerTapMs */
	2600,		/* chargeCommitCooldown */
	1050,		/* stunChargeHoldMs */
	6000,		/* engageSwitchMinMs */
	0.45f,		/* engageSwitchMargin */
	1.00f,		/* engageWeightBias */
	0.60f,		/* engageWeightKi */
	0.60f,		/* engageWeightStamina */
	0.00f,		/* engageWeightHealth */
	0.00f,		/* engageWeightTargetState */
	1.20f,		/* engageWeightDistance */
	0.00f,		/* engageWeightEffect */
	3000.0f,	/* engageNeutralDist */
	0.0f,		/* engageKiteMinDist */
	85,		/* recoverRestExitPct */
	45,		/* recoverRechargeMinStaminaPct */
	100,		/* recoverRechargeKiPct */
	4000,		/* recoverCooldownMs */
	20000,		/* transformIntervalMs */
	130,		/* tierEscalateMarginPct */
	8000,		/* tierEscalateCombatMs */
	30,		/* escapeCriticalHealthPct */
	0.0f,		/* meleeApproachMaxPitch */
	48.0f,		/* meleeApproachLevelTolerance */
	0.45f		/* meleeApproachSteepRatio */
};

static const botlite_profile_t botlite_skill2_profile_default = {
	10000.0f,
	64.0f,
	10000.0f,
	10000.0f,
	2200.0f,
	3000,
	450,
	3000,
	35.0f,
	120.0f,
	1000000.0f,
	10000.0f,
	10000.0f,
	1500.0f,
	5000,
	1200,
	5000,
	25,
	3000,
	1000,
	2000,
	0.50f,
	0.75f,
	900,
	1800,
	220,
	420,
	3000,
	3000,
	1000,
	650,
	2000,
	0.80f,
	12000.0f,
	0.0f,
	qfalse,
	0.0f,
	0.0f,
	220,		/* reactiveBlockMinReaction */
	520,		/* reactiveBlockMaxReaction */
	70,		/* staminaComfortablePct */
	30,		/* staminaTightPct */
	10,		/* staminaCriticalPct */
	500.0f,	/* zanzokenEscapeDist */
	1200.0f,	/* kiChargeMinSafeDist */
	60,		/* kiChargeTargetPct */
	28,		/* recoverEnterPct */
	62,		/* recoverExitPct */
	40,		/* recoverHealthEnterPct */
	70,		/* recoverHealthExitPct */
	1500.0f,	/* sanzokenBandMinDist */
	16000.0f,	/* sanzokenBandMaxDist */
	12000,	/* recoverMaxMs */
	900.0f,	/* recoverSafeDist */
	22,		/* recoverKiFloorPct */
	4,		/* recoverKiFloorHiddenPct */
	85,		/* breakLimitMinKiPct */
	550,		/* breakLimitHoldMs */
	40,		/* struggleCommitPct */
	130,		/* breakerTapMs */
	2600,		/* chargeCommitCooldown */
	1050,		/* stunChargeHoldMs */
	4000,		/* engageSwitchMinMs */
	0.35f,		/* engageSwitchMargin */
	1.00f,		/* engageWeightBias */
	0.90f,		/* engageWeightKi */
	0.80f,		/* engageWeightStamina */
	0.40f,		/* engageWeightHealth */
	0.60f,		/* engageWeightTargetState */
	0.90f,		/* engageWeightDistance */
	0.00f,		/* engageWeightEffect */
	2500.0f,	/* engageNeutralDist */
	0.0f,		/* engageKiteMinDist */
	85,		/* recoverRestExitPct */
	45,		/* recoverRechargeMinStaminaPct */
	100,		/* recoverRechargeKiPct */
	4000,		/* recoverCooldownMs */
	20000,		/* transformIntervalMs */
	130,		/* tierEscalateMarginPct */
	8000,		/* tierEscalateCombatMs */
	30,		/* escapeCriticalHealthPct */
	0.0f,		/* meleeApproachMaxPitch */
	48.0f,		/* meleeApproachLevelTolerance */
	0.45f		/* meleeApproachSteepRatio */
};

static const botlite_profile_t botlite_skill3_profile_default = {
	17000.0f,
	64.0f,
	17000.0f,
	17000.0f,
	2200.0f,
	3000,
	450,
	3000,
	35.0f,
	120.0f,
	1000000.0f,
	17000.0f,
	18500.0f,
	1500.0f,
	5000,
	1200,
	5000,
	25,
	3000,
	1000,
	2000,
	0.50f,
	0.75f,
	650,
	1300,
	280,
	560,
	3000,
	3000,
	1000,
	650,
	2000,
	0.80f,
	12000.0f,
	0.0f,
	qfalse,
	0.0f,
	0.0f,
	220,		/* reactiveBlockMinReaction */
	520,		/* reactiveBlockMaxReaction */
	70,		/* staminaComfortablePct */
	30,		/* staminaTightPct */
	10,		/* staminaCriticalPct */
	500.0f,	/* zanzokenEscapeDist */
	1200.0f,	/* kiChargeMinSafeDist */
	60,		/* kiChargeTargetPct */
	28,		/* recoverEnterPct */
	62,		/* recoverExitPct */
	40,		/* recoverHealthEnterPct */
	70,		/* recoverHealthExitPct */
	1500.0f,	/* sanzokenBandMinDist */
	16000.0f,	/* sanzokenBandMaxDist */
	12000,	/* recoverMaxMs */
	900.0f,	/* recoverSafeDist */
	22,		/* recoverKiFloorPct */
	4,		/* recoverKiFloorHiddenPct */
	85,		/* breakLimitMinKiPct */
	550,		/* breakLimitHoldMs */
	20,		/* struggleCommitPct */
	130,		/* breakerTapMs */
	2600,		/* chargeCommitCooldown */
	1050,		/* stunChargeHoldMs */
	2200,		/* engageSwitchMinMs */
	0.22f,		/* engageSwitchMargin */
	1.00f,		/* engageWeightBias */
	1.10f,		/* engageWeightKi */
	1.00f,		/* engageWeightStamina */
	0.90f,		/* engageWeightHealth */
	1.30f,		/* engageWeightTargetState */
	0.60f,		/* engageWeightDistance */
	0.50f,		/* engageWeightEffect */
	2200.0f,	/* engageNeutralDist */
	900.0f,		/* engageKiteMinDist */
	85,		/* recoverRestExitPct */
	45,		/* recoverRechargeMinStaminaPct */
	100,		/* recoverRechargeKiPct */
	4000,		/* recoverCooldownMs */
	20000,		/* transformIntervalMs */
	130,		/* tierEscalateMarginPct */
	8000,		/* tierEscalateCombatMs */
	30,		/* escapeCriticalHealthPct */
	0.0f,		/* meleeApproachMaxPitch */
	48.0f,		/* meleeApproachLevelTolerance */
	0.45f		/* meleeApproachSteepRatio */
};

static const botlite_combat_policy_t botlite_skill1_policy_default = {
	qtrue,
	qfalse,
	qfalse,
	qtrue,
	qfalse,
	qtrue,
	qfalse,
	qfalse,
	qfalse,
	qfalse,
	50,
	qtrue,	/* allowsReactiveBlock */
	qfalse,	/* allowsDeliberateCharge (skill1: nunca) */
	qfalse,	/* allowsBreaker (skill1: nunca) */
	qfalse,	/* allowsDefensiveZanzoken */
	qfalse,	/* allowsKiCharge */
	qfalse,	/* allowsRecoverMode (skill1: nunca) */
	qfalse,	/* allowsEvade */
	qfalse,	/* allowsDodgeIncoming (skill1: no) */
	qfalse,	/* allowsOffensiveBreakLimit */
	qfalse,	/* allowsEngageIntent (skill1: no, se queda con la logica por distancia) */
	qfalse	/* allowsEngageKiting (skill1: no) */
};

static const botlite_combat_policy_t botlite_skill2_policy_default = {
	qtrue,
	qtrue,
	qfalse,
	qtrue,
	qtrue,
	qtrue,
	qfalse,
	qfalse,
	qfalse,
	qfalse,
	50,
	qtrue,	/* allowsReactiveBlock */
	qtrue,	/* allowsDeliberateCharge */
	qtrue,	/* allowsBreaker */
	qtrue,	/* allowsDefensiveZanzoken */
	qtrue,	/* allowsKiCharge */
	qtrue,	/* allowsRecoverMode */
	qfalse,	/* allowsEvade (skill2: no) */
	qtrue,	/* allowsDodgeIncoming (skill2: si) */
	qfalse,	/* allowsOffensiveBreakLimit (skill2: no) */
	qtrue,	/* allowsEngageIntent (skill2: si, pero lento) */
	qfalse	/* allowsEngageKiting (skill2: no abre hueco a proposito) */
};

static const botlite_combat_policy_t botlite_skill3_policy_default = {
	qtrue,
	qtrue,
	qtrue,
	qtrue,
	qtrue,
	qtrue,
	qtrue,
	qtrue,
	qtrue,
	qtrue,
	50,
	qtrue,	/* allowsReactiveBlock */
	qtrue,	/* allowsDeliberateCharge */
	qtrue,	/* allowsBreaker */
	qtrue,	/* allowsDefensiveZanzoken */
	qtrue,	/* allowsKiCharge */
	qtrue,	/* allowsRecoverMode */
	qtrue,	/* allowsEvade (skill3: si) */
	qtrue,	/* allowsDodgeIncoming */
	qtrue,	/* allowsOffensiveBreakLimit (skill3: si) */
	qtrue,	/* allowsEngageIntent (skill3: si) */
	qtrue	/* allowsEngageKiting (skill3: si) */
};

static botlite_profile_t botlite_profiles[4];
static botlite_combat_policy_t botlite_policies[4];
static botlite_archetype_cfg_t botlite_archetypes[BOTLITE_MAX_ARCHETYPES];
static botlite_botdef_t botlite_botdefs[BOTLITE_MAX_BOT_DEFS];
static qboolean botlite_botsysLoaded = qfalse;
static char botlite_skillConfigBuffer[BOTLITE_FILE_BUFFER_SIZE];
static char botlite_archetypeBuffer[BOTLITE_FILE_BUFFER_SIZE];
static char botlite_botDefsBuffer[BOTLITE_FILE_BUFFER_SIZE];

static void BotLite_ApplyBalancedDefaults( botlite_info_t *info );
static qboolean BotLite_ReadTextFile( const char *path, char *buffer, int bufferSize );
static void BotLite_ResetSkillDefaults( void );
static void BotLite_ResetBotsysTables( void );
static void BotLite_TrimString( char *text );
static void BotLite_StripExtension( const char *src, char *dst, int dstSize );
static qboolean BotLite_StringMatchesToken( const char *lhs, const char *rhs );
static int BotLite_ProfileClampSkill( int skill );
static qboolean BotLite_ParseBool( const char *value );
static void BotLite_UpdatePolicyMeleeAvailability( botlite_combat_policy_t *policy );
static void BotLite_ApplySkillKeyValue( int skill, const char *key, const char *value );
static void BotLite_LoadSkillConfigFile( int skill, const char *path );
static int BotLite_FindArchetypeIndex( const char *name );
static int BotLite_LoadArchetypeFile( const char *path, const char *fallbackName );
static void BotLite_LoadKnownArchetypes( void );
static int BotLite_ParseSkillFromString( const char *value );
static void BotLite_LoadBotDefinitions( void );

static void BotLite_ApplyBalancedDefaults( botlite_info_t *info ) {
	if ( !info ) {
		return;
	}
	Q_strncpyz( info->archetypeName, "balanced", sizeof( info->archetypeName ) );
	info->aggression = 0.60f;
	info->rushTendency = 0.55f;
	info->blockTendency = 0.45f;
	info->specialTendency = 0.40f;
	info->rangedBias = 0.35f;
	info->comboCommitment = 0.55f;
	info->targetSwitchBias = 0.20f;
	info->powerlevelThreshold = 0.28f;
	info->recoverRespect = 0.70f;
}

static qboolean BotLite_ReadTextFile( const char *path, char *buffer, int bufferSize ) {
	fileHandle_t file;
	int len;

	if ( !path || !buffer || bufferSize <= 1 ) {
		return qfalse;
	}

	len = trap_FS_FOpenFile( path, &file, FS_READ );
	if ( len <= 0 || !file ) {
		return qfalse;
	}
	if ( len >= bufferSize ) {
		len = bufferSize - 1;
	}
	trap_FS_Read( buffer, len, file );
	buffer[len] = '\0';
	trap_FS_FCloseFile( file );
	return qtrue;
}

static void BotLite_ResetSkillDefaults( void ) {
	memset( botlite_profiles, 0, sizeof( botlite_profiles ) );
	memset( botlite_policies, 0, sizeof( botlite_policies ) );
	botlite_profiles[1] = botlite_skill1_profile_default;
	botlite_profiles[2] = botlite_skill2_profile_default;
	botlite_profiles[3] = botlite_skill3_profile_default;
	botlite_policies[1] = botlite_skill1_policy_default;
	botlite_policies[2] = botlite_skill2_policy_default;
	botlite_policies[3] = botlite_skill3_policy_default;
}

static void BotLite_ResetBotsysTables( void ) {
	memset( botlite_archetypes, 0, sizeof( botlite_archetypes ) );
	memset( botlite_botdefs, 0, sizeof( botlite_botdefs ) );
}

static void BotLite_TrimString( char *text ) {
	int len;
	char *start;

	if ( !text || !text[0] ) {
		return;
	}

	start = text;
	while ( *start == ' ' || *start == '\t' || *start == '\r' || *start == '\n' ) {
		start++;
	}
	if ( start != text ) {
		memmove( text, start, strlen( start ) + 1 );
	}

	len = strlen( text );
	while ( len > 0 ) {
		char c;
		c = text[len - 1];
		if ( c != ' ' && c != '\t' && c != '\r' && c != '\n' ) {
			break;
		}
		text[len - 1] = '\0';
		len--;
	}
}

static void BotLite_StripExtension( const char *src, char *dst, int dstSize ) {
	char temp[MAX_QPATH];
	char *dot;
	const char *name;

	if ( !dst || dstSize <= 0 ) {
		return;
	}
	dst[0] = '\0';
	if ( !src || !src[0] ) {
		return;
	}

	name = src;
	if ( Q_strrchr( src, '/' ) ) {
		name = Q_strrchr( src, '/' ) + 1;
	}
	if ( Q_strrchr( name, '\\' ) ) {
		name = Q_strrchr( name, '\\' ) + 1;
	}
	Q_strncpyz( temp, name, sizeof( temp ) );
	dot = Q_strrchr( temp, '.' );
	if ( dot ) {
		*dot = '\0';
	}
	Q_strncpyz( dst, temp, dstSize );
}

static qboolean BotLite_StringMatchesToken( const char *lhs, const char *rhs ) {
	char cleanLhs[MAX_QPATH];
	char cleanRhs[MAX_QPATH];

	if ( !lhs || !rhs || !lhs[0] || !rhs[0] ) {
		return qfalse;
	}
	if ( Q_stricmp( lhs, rhs ) == 0 ) {
		return qtrue;
	}
	BotLite_StripExtension( lhs, cleanLhs, sizeof( cleanLhs ) );
	BotLite_StripExtension( rhs, cleanRhs, sizeof( cleanRhs ) );
	return ( Q_stricmp( cleanLhs, cleanRhs ) == 0 ) ? qtrue : qfalse;
}

static int BotLite_ProfileClampSkill( int skill ) {
	if ( skill < 1 ) {
		return 1;
	}
	if ( skill > 3 ) {
		return 3;
	}
	return skill;
}

static qboolean BotLite_ParseBool( const char *value ) {
	if ( !value || !value[0] ) {
		return qfalse;
	}
	if ( !Q_stricmp( value, "true" ) || !Q_stricmp( value, "yes" ) ) {
		return qtrue;
	}
	return atoi( value ) != 0 ? qtrue : qfalse;
}

static void BotLite_UpdatePolicyMeleeAvailability( botlite_combat_policy_t *policy ) {
	if ( !policy ) {
		return;
	}
	policy->allowsMeleePressure = ( policy->directMeleePreference || policy->allowsRangedPressure ) ? qtrue : qfalse;
}

static void BotLite_ApplySkillKeyValue( int skill, const char *key, const char *value ) {
	botlite_profile_t *profile;
	botlite_combat_policy_t *policy;
	float f;
	int i;

	if ( skill < 1 || skill > 3 || !key || !value ) {
		return;
	}

	profile = &botlite_profiles[skill];
	policy = &botlite_policies[skill];
	f = atof( value );
	i = atoi( value );

	if ( !Q_stricmp( key, "think_tick_ms" ) ) {
		policy->tacticalThinkMs = i > 0 ? i : policy->tacticalThinkMs;
	} else if ( !Q_stricmp( key, "uses_mixed" ) || !Q_stricmp( key, "uses_ranged" ) ) {
		policy->allowsRangedPressure = BotLite_ParseBool( value );
		BotLite_UpdatePolicyMeleeAvailability( policy );
	} else if ( !Q_stricmp( key, "uses_transform" ) ) {
		policy->needsInitialTransform = BotLite_ParseBool( value );
	} else if ( !Q_stricmp( key, "uses_melee" ) ) {
		policy->directMeleePreference = BotLite_ParseBool( value );
		BotLite_UpdatePolicyMeleeAvailability( policy );
	} else if ( !Q_stricmp( key, "uses_spawn_flyup" ) ) {
		policy->useSearchFlyup = BotLite_ParseBool( value );
	} else if ( !Q_stricmp( key, "uses_opening_style" ) ) {
		policy->chooseOpeningStyle = BotLite_ParseBool( value );
	} else if ( !Q_stricmp( key, "knockback_cancel_delay_ms" ) ) {
		profile->skill3KnockbackCancelDelay = i;
	} else if ( !Q_stricmp( key, "uses_defensive_zanzoken" ) ) {
		policy->allowsDefensiveZanzoken = BotLite_ParseBool( value );
	} else if ( !Q_stricmp( key, "uses_offensive_breaklimit" ) ) {
		policy->allowsOffensiveBreakLimit = BotLite_ParseBool( value );
	} else if ( !Q_stricmp( key, "uses_dodge_incoming" ) ) {
		policy->allowsDodgeIncoming = BotLite_ParseBool( value );
	} else if ( !Q_stricmp( key, "uses_evade" ) ) {
		policy->allowsEvade = BotLite_ParseBool( value );
	} else if ( !Q_stricmp( key, "uses_recover_mode" ) ) {
		policy->allowsRecoverMode = BotLite_ParseBool( value );
	} else if ( !Q_stricmp( key, "uses_ki_charge" ) ) {
		policy->allowsKiCharge = BotLite_ParseBool( value );
	} else if ( !Q_stricmp( key, "uses_breakers" ) ) {
		policy->allowsBreaker = BotLite_ParseBool( value );
	} else if ( !Q_stricmp( key, "uses_charge_attacks" ) ) {
		policy->allowsDeliberateCharge = BotLite_ParseBool( value );
	} else if ( !Q_stricmp( key, "charge_cooldown_ms" ) ) {
		profile->chargeCommitCooldown = i;
	} else if ( !Q_stricmp( key, "stun_charge_hold_ms" ) ) {
		profile->stunChargeHoldMs = i;
	} else if ( !Q_stricmp( key, "stamina_comfortable_pct" ) ) {
		profile->staminaComfortablePct = i;
	} else if ( !Q_stricmp( key, "stamina_tight_pct" ) ) {
		profile->staminaTightPct = i;
	} else if ( !Q_stricmp( key, "stamina_critical_pct" ) ) {
		profile->staminaCriticalPct = i;
	} else if ( !Q_stricmp( key, "zanzoken_escape_dist" ) ) {
		profile->zanzokenEscapeDist = f;
	} else if ( !Q_stricmp( key, "ki_charge_min_safe_dist" ) ) {
		profile->kiChargeMinSafeDist = f;
	} else if ( !Q_stricmp( key, "ki_charge_target_pct" ) ) {
		profile->kiChargeTargetPct = i;
	} else if ( !Q_stricmp( key, "recover_enter_pct" ) ) {
		profile->recoverEnterPct = i;
	} else if ( !Q_stricmp( key, "recover_exit_pct" ) ) {
		profile->recoverExitPct = i;
	} else if ( !Q_stricmp( key, "recover_health_enter_pct" ) ) {
		profile->recoverHealthEnterPct = i;
	} else if ( !Q_stricmp( key, "recover_health_exit_pct" ) ) {
		profile->recoverHealthExitPct = i;
	} else if ( !Q_stricmp( key, "sanzoken_band_min_dist" ) ) {
		profile->sanzokenBandMinDist = f;
	} else if ( !Q_stricmp( key, "sanzoken_band_max_dist" ) ) {
		profile->sanzokenBandMaxDist = f;
	} else if ( !Q_stricmp( key, "recover_max_ms" ) ) {
		profile->recoverMaxMs = i;
	} else if ( !Q_stricmp( key, "recover_safe_dist" ) ) {
		profile->recoverSafeDist = f;
	} else if ( !Q_stricmp( key, "recover_ki_floor_pct" ) ) {
		profile->recoverKiFloorPct = i;
	} else if ( !Q_stricmp( key, "recover_ki_floor_hidden_pct" ) ) {
		profile->recoverKiFloorHiddenPct = i;
	} else if ( !Q_stricmp( key, "breaklimit_min_ki_pct" ) ) {
		profile->breakLimitMinKiPct = i;
	} else if ( !Q_stricmp( key, "breaklimit_hold_ms" ) ) {
		profile->breakLimitHoldMs = i;
	} else if ( !Q_stricmp( key, "struggle_commit_pct" ) ) {
		profile->struggleCommitPct = i;
	} else if ( !Q_stricmp( key, "breaker_tap_ms" ) ) {
		profile->breakerTapMs = i;
	} else if ( !Q_stricmp( key, "melee_approach_max_pitch" ) ) {
		profile->meleeApproachMaxPitch = f;
	} else if ( !Q_stricmp( key, "melee_approach_level_tolerance" ) ) {
		profile->meleeApproachLevelTolerance = f;
	} else if ( !Q_stricmp( key, "melee_approach_steep_ratio" ) ) {
		profile->meleeApproachSteepRatio = f;
	} else if ( !Q_stricmp( key, "transform_interval_ms" ) ) {
		profile->transformIntervalMs = i;
	} else if ( !Q_stricmp( key, "tier_escalate_margin_pct" ) ) {
		profile->tierEscalateMarginPct = i;
	} else if ( !Q_stricmp( key, "tier_escalate_combat_ms" ) ) {
		profile->tierEscalateCombatMs = i;
	} else if ( !Q_stricmp( key, "escape_critical_health_pct" ) ) {
		profile->escapeCriticalHealthPct = i;
	} else if ( !Q_stricmp( key, "recover_rest_exit_pct" ) ) {
		profile->recoverRestExitPct = i;
	} else if ( !Q_stricmp( key, "recover_recharge_min_stamina_pct" ) ) {
		profile->recoverRechargeMinStaminaPct = i;
	} else if ( !Q_stricmp( key, "recover_recharge_ki_pct" ) ) {
		profile->recoverRechargeKiPct = i;
	} else if ( !Q_stricmp( key, "recover_cooldown_ms" ) ) {
		profile->recoverCooldownMs = i;
	} else if ( !Q_stricmp( key, "uses_engage_intent" ) ) {
		policy->allowsEngageIntent = BotLite_ParseBool( value );
	} else if ( !Q_stricmp( key, "uses_engage_kiting" ) ) {
		policy->allowsEngageKiting = BotLite_ParseBool( value );
	} else if ( !Q_stricmp( key, "engage_switch_min_ms" ) ) {
		profile->engageSwitchMinMs = i;
	} else if ( !Q_stricmp( key, "engage_switch_margin" ) ) {
		profile->engageSwitchMargin = f;
	} else if ( !Q_stricmp( key, "engage_w_bias" ) ) {
		profile->engageWeightBias = f;
	} else if ( !Q_stricmp( key, "engage_w_ki" ) ) {
		profile->engageWeightKi = f;
	} else if ( !Q_stricmp( key, "engage_w_stamina" ) ) {
		profile->engageWeightStamina = f;
	} else if ( !Q_stricmp( key, "engage_w_health" ) ) {
		profile->engageWeightHealth = f;
	} else if ( !Q_stricmp( key, "engage_w_target_state" ) ) {
		profile->engageWeightTargetState = f;
	} else if ( !Q_stricmp( key, "engage_w_distance" ) ) {
		profile->engageWeightDistance = f;
	} else if ( !Q_stricmp( key, "engage_w_effect" ) ) {
		profile->engageWeightEffect = f;
	} else if ( !Q_stricmp( key, "engage_neutral_dist" ) ) {
		profile->engageNeutralDist = f;
	} else if ( !Q_stricmp( key, "engage_kite_min_dist" ) ) {
		profile->engageKiteMinDist = f;
	} else if ( !Q_stricmp( key, "uses_reactive_block" ) ) {
		policy->allowsReactiveBlock = BotLite_ParseBool( value );
	} else if ( !Q_stricmp( key, "reactive_block_min_ms" ) ) {
		profile->reactiveBlockMinReaction = i;
	} else if ( !Q_stricmp( key, "reactive_block_max_ms" ) ) {
		profile->reactiveBlockMaxReaction = i;
	} else if ( !Q_stricmp( key, "uses_block" ) ) {
		policy->allowsBlock = BotLite_ParseBool( value );
	} else if ( !Q_stricmp( key, "uses_special" ) ) {
		policy->allowsSpecial = BotLite_ParseBool( value );
	} else if ( !Q_stricmp( key, "uses_sanzoken" ) ) {
		policy->allowsApproachSanzoken = BotLite_ParseBool( value );
	} else if ( !Q_stricmp( key, "target_acquire_min_distance" ) ) {
		profile->targetAcquireMinDistance = f > 0.0f ? f : 0.0f;
	} else if ( !Q_stricmp( key, "target_acquire_max_distance" ) ) {
		profile->acquireDistance = f > 0.0f ? f : 0.0f;
	} else if ( !Q_stricmp( key, "target_acquire_requires_los" ) ) {
		profile->targetAcquireRequiresLOS = BotLite_ParseBool( value );
	} else if ( !Q_stricmp( key, "ranged_to_melee_distance" ) ) {
		profile->rangedToMeleeDistance = f;
	} else if ( !Q_stricmp( key, "melee_to_ranged_distance" ) ) {
		profile->meleeToRangedDistance = f;
		profile->skill2RangedDistance = f;
		profile->skill2MeleeExitDistance = f;
	} else if ( !Q_stricmp( key, "recover_wait_ms" ) ) {
		profile->recoveryRetreatTime = i;
	} else if ( !Q_stricmp( key, "sanzoken_interval_ms" ) ) {
		profile->skill3DodgeMinInterval = i;
		profile->skill3DodgeMaxInterval = i;
	} else if ( !Q_stricmp( key, "block_reaction_min_ms" ) ) {
		profile->skill3BlockMinInterval = i;
	} else if ( !Q_stricmp( key, "block_reaction_max_ms" ) ) {
		profile->skill3BlockMaxInterval = i;
	} else if ( !Q_stricmp( key, "special_hold_ms" ) ) {
		profile->skill3PowerMeleeChargeTime = i;
	}
}

static void BotLite_LoadSkillConfigFile( int skill, const char *path ) {
	char *line;
	char *cursor;

	if ( !path || !path[0] ) {
		return;
	}
	if ( !BotLite_ReadTextFile( path, botlite_skillConfigBuffer, sizeof( botlite_skillConfigBuffer ) ) ) {
		return;
	}

	cursor = botlite_skillConfigBuffer;
	while ( cursor && cursor[0] ) {
		char *nextLine;
		char *eq;
		char key[128];
		char value[128];

		nextLine = strchr( cursor, '\n' );
		if ( nextLine ) {
			*nextLine = '\0';
			line = cursor;
			cursor = nextLine + 1;
		} else {
			line = cursor;
			cursor = NULL;
		}

		BotLite_TrimString( line );
		if ( !line[0] || line[0] == '#' ) {
			continue;
		}

		eq = strchr( line, '=' );
		if ( !eq ) {
			continue;
		}
		*eq = '\0';
		Q_strncpyz( key, line, sizeof( key ) );
		Q_strncpyz( value, eq + 1, sizeof( value ) );
		BotLite_TrimString( key );
		BotLite_TrimString( value );
		BotLite_ApplySkillKeyValue( skill, key, value );
	}
}

static int BotLite_FindArchetypeIndex( const char *name ) {
	int i;

	if ( !name || !name[0] ) {
		return -1;
	}

	for ( i = 0; i < BOTLITE_MAX_ARCHETYPES; i++ ) {
		if ( !botlite_archetypes[i].inuse ) {
			continue;
		}
		if ( BotLite_StringMatchesToken( name, botlite_archetypes[i].sourceName ) ||
			 BotLite_StringMatchesToken( name, botlite_archetypes[i].name ) ) {
			return i;
		}
	}
	return -1;
}

static int BotLite_LoadArchetypeFile( const char *path, const char *fallbackName ) {
	char *cursor;
	int index;
	int i;
	char sourceName[MAX_QPATH];

	if ( !path || !path[0] ) {
		return -1;
	}

	BotLite_StripExtension( fallbackName ? fallbackName : path, sourceName, sizeof( sourceName ) );
	index = BotLite_FindArchetypeIndex( sourceName );
	if ( index >= 0 ) {
		return index;
	}

	if ( !BotLite_ReadTextFile( path, botlite_archetypeBuffer, sizeof( botlite_archetypeBuffer ) ) ) {
		return -1;
	}

	index = -1;
	for ( i = 0; i < BOTLITE_MAX_ARCHETYPES; i++ ) {
		if ( !botlite_archetypes[i].inuse ) {
			index = i;
			break;
		}
	}
	if ( index < 0 ) {
		return -1;
	}

	memset( &botlite_archetypes[index], 0, sizeof( botlite_archetypes[index] ) );
	botlite_archetypes[index].inuse = qtrue;
	Q_strncpyz( botlite_archetypes[index].sourceName, sourceName, sizeof( botlite_archetypes[index].sourceName ) );
	Q_strncpyz( botlite_archetypes[index].name, sourceName, sizeof( botlite_archetypes[index].name ) );
	botlite_archetypes[index].aggression = 0.60f;
	botlite_archetypes[index].rushTendency = 0.55f;
	botlite_archetypes[index].blockTendency = 0.45f;
	botlite_archetypes[index].specialTendency = 0.40f;
	botlite_archetypes[index].rangedBias = 0.35f;
	botlite_archetypes[index].comboCommitment = 0.55f;
	botlite_archetypes[index].targetSwitchBias = 0.20f;
	botlite_archetypes[index].powerlevelThreshold = 0.28f;
	botlite_archetypes[index].recoverRespect = 0.70f;

	cursor = botlite_archetypeBuffer;
	while ( cursor && cursor[0] ) {
		char *nextLine;
		char *eq;
		char *line;
		char key[128];
		char value[128];

		nextLine = strchr( cursor, '\n' );
		if ( nextLine ) {
			*nextLine = '\0';
			line = cursor;
			cursor = nextLine + 1;
		} else {
			line = cursor;
			cursor = NULL;
		}

		BotLite_TrimString( line );
		if ( !line[0] || line[0] == '#' ) {
			continue;
		}

		eq = strchr( line, '=' );
		if ( !eq ) {
			continue;
		}
		*eq = '\0';
		Q_strncpyz( key, line, sizeof( key ) );
		Q_strncpyz( value, eq + 1, sizeof( value ) );
		BotLite_TrimString( key );
		BotLite_TrimString( value );

		if ( !Q_stricmp( key, "name" ) ) {
			Q_strncpyz( botlite_archetypes[index].name, value, sizeof( botlite_archetypes[index].name ) );
		} else if ( !Q_stricmp( key, "aggression" ) ) {
			botlite_archetypes[index].aggression = atof( value );
		} else if ( !Q_stricmp( key, "rush_tendency" ) ) {
			botlite_archetypes[index].rushTendency = atof( value );
		} else if ( !Q_stricmp( key, "block_tendency" ) ) {
			botlite_archetypes[index].blockTendency = atof( value );
		} else if ( !Q_stricmp( key, "special_tendency" ) ) {
			botlite_archetypes[index].specialTendency = atof( value );
		} else if ( !Q_stricmp( key, "ranged_bias" ) ) {
			botlite_archetypes[index].rangedBias = atof( value );
		} else if ( !Q_stricmp( key, "combo_commitment" ) ) {
			botlite_archetypes[index].comboCommitment = atof( value );
		} else if ( !Q_stricmp( key, "target_switch_bias" ) ) {
			botlite_archetypes[index].targetSwitchBias = atof( value );
		} else if ( !Q_stricmp( key, "powerlevel_threshold" ) ) {
			botlite_archetypes[index].powerlevelThreshold = atof( value );
		} else if ( !Q_stricmp( key, "recover_respect" ) ) {
			botlite_archetypes[index].recoverRespect = atof( value );
		}
	}

	return index;
}

static void BotLite_LoadKnownArchetypes( void ) {
	static const char *knownArchetypes[] = {
		"balanced.cfg",
		"aggressive.cfg",
		"tactical.cfg",
		"keepaway.cfg",
		"mobile.cfg",
		"bruiser.cfg"
	};
	int i;
	char path[MAX_QPATH];

	for ( i = 0; i < (int)( sizeof( knownArchetypes ) / sizeof( knownArchetypes[0] ) ); i++ ) {
		Com_sprintf( path, sizeof( path ), "botsys/archetypes/%s", knownArchetypes[i] );
		BotLite_LoadArchetypeFile( path, knownArchetypes[i] );
	}
}

static int BotLite_ParseSkillFromString( const char *value ) {
	if ( !value || !value[0] ) {
		return 0;
	}
	if ( strstr( value, "skill1" ) || strstr( value, "SKILL1" ) ) {
		return 1;
	}
	if ( strstr( value, "skill2" ) || strstr( value, "SKILL2" ) ) {
		return 2;
	}
	if ( strstr( value, "skill3" ) || strstr( value, "SKILL3" ) ) {
		return 3;
	}
	return BotLite_ProfileClampSkill( atoi( value ) );
}

static void BotLite_LoadBotDefinitions( void ) {
	char *cursor;
	int count;

	if ( !BotLite_ReadTextFile( "botsys/bots/bots.txt", botlite_botDefsBuffer, sizeof( botlite_botDefsBuffer ) ) ) {
		return;
	}

	cursor = botlite_botDefsBuffer;
	count = 0;
	while ( cursor && cursor[0] && count < BOTLITE_MAX_BOT_DEFS ) {
		char *nextLine;
		char *line;
		char temp[512];
		char *segmentStart;
		botlite_botdef_t *def;

		nextLine = strchr( cursor, '\n' );
		if ( nextLine ) {
			*nextLine = '\0';
			line = cursor;
			cursor = nextLine + 1;
		} else {
			line = cursor;
			cursor = NULL;
		}

		BotLite_TrimString( line );
		if ( !line[0] || line[0] == '#' ) {
			continue;
		}

		def = &botlite_botdefs[count];
		memset( def, 0, sizeof( *def ) );
		def->skill = 0;
		Q_strncpyz( temp, line, sizeof( temp ) );
		segmentStart = temp;
		while ( segmentStart && segmentStart[0] ) {
			char *segmentEnd;
			char *eq;
			char key[128];
			char value[128];

			segmentEnd = strchr( segmentStart, ';' );
			if ( segmentEnd ) {
				*segmentEnd = '\0';
			}
			BotLite_TrimString( segmentStart );
			eq = strchr( segmentStart, '=' );
			if ( eq ) {
				*eq = '\0';
				Q_strncpyz( key, segmentStart, sizeof( key ) );
				Q_strncpyz( value, eq + 1, sizeof( value ) );
				BotLite_TrimString( key );
				BotLite_TrimString( value );

				if ( !Q_stricmp( key, "bot" ) ) {
					Q_strncpyz( def->id, value, sizeof( def->id ) );
				} else if ( !Q_stricmp( key, "name" ) ) {
					Q_strncpyz( def->displayName, value, sizeof( def->displayName ) );
				} else if ( !Q_stricmp( key, "player" ) ) {
					Q_strncpyz( def->playerModel, value, sizeof( def->playerModel ) );
				} else if ( !Q_stricmp( key, "archetype" ) ) {
					Q_strncpyz( def->archetype, value, sizeof( def->archetype ) );
				} else if ( !Q_stricmp( key, "skill" ) ) {
					def->skill = BotLite_ParseSkillFromString( value );
				}
			}

			if ( !segmentEnd ) {
				break;
			}
			segmentStart = segmentEnd + 1;
		}

		if ( def->id[0] || def->playerModel[0] || def->displayName[0] ) {
			if ( !def->displayName[0] ) {
				Q_strncpyz( def->displayName, def->id[0] ? def->id : def->playerModel, sizeof( def->displayName ) );
			}
			if ( !def->playerModel[0] ) {
				Q_strncpyz( def->playerModel, def->id, sizeof( def->playerModel ) );
			}
			def->inuse = qtrue;
			count++;
			if ( def->archetype[0] ) {
				char path[MAX_QPATH];
				Com_sprintf( path, sizeof( path ), "botsys/archetypes/%s", def->archetype );
				BotLite_LoadArchetypeFile( path, def->archetype );
			}
		}
	}
}

void BotLite_BotsysInit( void ) {
	if ( botlite_botsysLoaded ) {
		return;
	}

	BotLite_ResetSkillDefaults();
	BotLite_ResetBotsysTables();
	BotLite_LoadSkillConfigFile( 1, "botsys/skills/defaults.cfg" );
	BotLite_LoadSkillConfigFile( 2, "botsys/skills/defaults.cfg" );
	BotLite_LoadSkillConfigFile( 3, "botsys/skills/defaults.cfg" );
	BotLite_LoadSkillConfigFile( 1, "botsys/skills/skill1.cfg" );
	BotLite_LoadSkillConfigFile( 2, "botsys/skills/skill2.cfg" );
	BotLite_LoadSkillConfigFile( 3, "botsys/skills/skill3.cfg" );
	BotLite_LoadKnownArchetypes();
	BotLite_LoadBotDefinitions();
	botlite_botsysLoaded = qtrue;
}

qboolean BotLite_BotsysResolveBot( const char *characterName, int requestedSkill, char *outDisplayName, int displaySize, char *outModelName, int modelSize, int *outResolvedSkill, char *outArchetype, int archetypeSize ) {
	int i;

	BotLite_BotsysInit();

	if ( outDisplayName && displaySize > 0 ) {
		Q_strncpyz( outDisplayName, characterName ? characterName : "bot", displaySize );
	}
	if ( outModelName && modelSize > 0 ) {
		Q_strncpyz( outModelName, characterName ? characterName : "bot", modelSize );
	}
	if ( outArchetype && archetypeSize > 0 ) {
		outArchetype[0] = '\0';
	}
	if ( outResolvedSkill ) {
		*outResolvedSkill = requestedSkill > 0 ? BotLite_ProfileClampSkill( requestedSkill ) : 1;
	}
	if ( !characterName || !characterName[0] ) {
		return qfalse;
	}

	for ( i = 0; i < BOTLITE_MAX_BOT_DEFS; i++ ) {
		if ( !botlite_botdefs[i].inuse ) {
			continue;
		}
		if ( !BotLite_StringMatchesToken( characterName, botlite_botdefs[i].id ) &&
			 !BotLite_StringMatchesToken( characterName, botlite_botdefs[i].displayName ) &&
			 !BotLite_StringMatchesToken( characterName, botlite_botdefs[i].playerModel ) ) {
			continue;
		}

		if ( outDisplayName && displaySize > 0 ) {
			Q_strncpyz( outDisplayName, botlite_botdefs[i].displayName, displaySize );
		}
		if ( outModelName && modelSize > 0 ) {
			Q_strncpyz( outModelName, botlite_botdefs[i].playerModel, modelSize );
		}
		if ( outArchetype && archetypeSize > 0 ) {
			Q_strncpyz( outArchetype, botlite_botdefs[i].archetype, archetypeSize );
		}
		if ( outResolvedSkill ) {
			*outResolvedSkill = requestedSkill > 0 ? BotLite_ProfileClampSkill( requestedSkill ) : ( botlite_botdefs[i].skill > 0 ? botlite_botdefs[i].skill : 1 );
		}
		return qtrue;
	}

	return qfalse;
}

void BotLite_BotsysApplyInfoConfig( botlite_info_t *info, const char *archetypeName ) {
	int index;

	if ( !info ) {
		return;
	}

	BotLite_BotsysInit();
	BotLite_ApplyBalancedDefaults( info );
	if ( !archetypeName || !archetypeName[0] ) {
		return;
	}

	index = BotLite_FindArchetypeIndex( archetypeName );
	if ( index < 0 ) {
		char path[MAX_QPATH];
		Com_sprintf( path, sizeof( path ), "botsys/archetypes/%s", archetypeName );
		index = BotLite_LoadArchetypeFile( path, archetypeName );
	}
	if ( index < 0 ) {
		return;
	}

	Q_strncpyz( info->archetypeName, botlite_archetypes[index].name, sizeof( info->archetypeName ) );
	info->aggression = botlite_archetypes[index].aggression;
	info->rushTendency = botlite_archetypes[index].rushTendency;
	info->blockTendency = botlite_archetypes[index].blockTendency;
	info->specialTendency = botlite_archetypes[index].specialTendency;
	info->rangedBias = botlite_archetypes[index].rangedBias;
	info->comboCommitment = botlite_archetypes[index].comboCommitment;
	info->targetSwitchBias = botlite_archetypes[index].targetSwitchBias;
	info->powerlevelThreshold = botlite_archetypes[index].powerlevelThreshold;
	info->recoverRespect = botlite_archetypes[index].recoverRespect;
}

const botlite_profile_t *BotLite_GetProfile( int skill ) {
	BotLite_BotsysInit();
	skill = BotLite_ProfileClampSkill( skill );
	return &botlite_profiles[skill];
}

const botlite_combat_policy_t *BotLite_GetCombatPolicy( int skill ) {
	BotLite_BotsysInit();
	skill = BotLite_ProfileClampSkill( skill );
	return &botlite_policies[skill];
}

botlite_mode_t BotLite_DefaultModeForSkill( int skill ) {
	if ( skill >= 1 && skill <= 3 ) {
		return BOTLITE_MODE_SEARCH;
	}
	return BOTLITE_MODE_IDLE;
}
