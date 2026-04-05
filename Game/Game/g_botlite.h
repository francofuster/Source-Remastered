#ifndef G_BOTLITE_H
#define G_BOTLITE_H

#define BOTLITE_MAX_BOTS MAX_CLIENTS
#define BOTLITE_VIEW_HEIGHT 48.0f

typedef enum {
	BOTLITE_MODE_IDLE = 0,
	BOTLITE_MODE_SEARCH,
	BOTLITE_MODE_COMBAT,
	BOTLITE_MODE_WAIT_TARGET_RECOVERY
} botlite_mode_t;

typedef enum {
	BOTLITE_GOAL_NONE = 0,
	BOTLITE_GOAL_SEARCH,
	BOTLITE_GOAL_COMBAT,
	BOTLITE_GOAL_WAIT_RECOVERY,
	BOTLITE_GOAL_POST_CRASH_FLYUP,
	BOTLITE_GOAL_IDLE
} botlite_goal_t;

typedef enum {
	BOTLITE_TACTIC_NONE = 0,
	BOTLITE_TACTIC_SEARCH_PATTERN,
	BOTLITE_TACTIC_APPROACH,
	BOTLITE_TACTIC_MELEE_PRESSURE,
	BOTLITE_TACTIC_RANGED_PRESSURE,
	BOTLITE_TACTIC_PUNISH_RECOVERY,
	BOTLITE_TACTIC_RETREAT_RECOVERY,
	BOTLITE_TACTIC_REPOSITION,
	BOTLITE_TACTIC_IDLE_TRACK
} botlite_tactic_t;

typedef enum {
	BOTLITE_COMBO_PUNCH = 0,
	BOTLITE_COMBO_KICK,
	BOTLITE_COMBO_SPEED,
	BOTLITE_COMBO_FINISH
} botlite_combo_stage_t;

typedef enum {
	BOTLITE_COMBAT_STATE_NONE = 0,
	BOTLITE_COMBAT_STATE_APPROACH,
	BOTLITE_COMBAT_STATE_SANZOKEN_APPROACH,
	BOTLITE_COMBAT_STATE_RANGED_PRESSURE,
	BOTLITE_COMBAT_STATE_MELEE_COMBO,
	BOTLITE_COMBAT_STATE_BLOCK,
	BOTLITE_COMBAT_STATE_SPECIAL,
	BOTLITE_COMBAT_STATE_FROZEN
} botlite_combat_state_t;

typedef struct {
	qboolean useSearchPattern;
	qboolean useSearchFlyup;
	qboolean needsInitialTransform;
	qboolean allowsMeleePressure;
	qboolean allowsRangedPressure;
	qboolean directMeleePreference;
	qboolean allowsApproachSanzoken;
	qboolean allowsBlock;
	qboolean allowsSpecial;
	qboolean chooseOpeningStyle;
	int tacticalThinkMs;
} botlite_combat_policy_t;

typedef struct {
	float acquireDistance;
	float combatDistance;
	float meleeStartDistance;
	float meleeChaseDistance;
	float lockRange;
	int searchRiseTime;
	int searchTurnTime;
	int searchForwardTime;
	float searchTurnMin;
	float searchTurnMax;
	float skill2AcquireDistance;
	float skill2RangedDistance;
	float skill2MeleeExitDistance;
	float skill2ApproachTriggerDistance;
	int skill2ApproachInterval;
	int skill2ApproachTime;
	int skill2WeaponSwitchTime;
	int skill2ChargeMin;
	int recoveryRetreatTime;
	int postCrashFlyupTime;
	int searchFlyupTime;
	float skill3HealthStartPct;
	float skill3HealthStopPct;
	int skill3BlockMinInterval;
	int skill3BlockMaxInterval;
	int skill3BlockMinDuration;
	int skill3BlockMaxDuration;
	int skill3DodgeMinInterval;
	int skill3DodgeMaxInterval;
	int skill3DodgeDuration;
	int skill3PowerMeleeChargeTime;
	int skill3KnockbackCancelDelay;
	float rangedChargeReleasePct;
	float skill3HealRetreatMaxDistance;
	float targetAcquireMinDistance;
	qboolean targetAcquireRequiresLOS;
	float rangedToMeleeDistance;
	float meleeToRangedDistance;
} botlite_profile_t;

typedef struct {
	int forwardmove;
	int rightmove;
	int upmove;
	int buttons;
	int weapon;
	qboolean weaponOverride;
} botlite_action_t;

typedef struct {
	botlite_mode_t mode;
	botlite_goal_t lastGoal;
	botlite_tactic_t lastTactic;
	int lastTargetNum;
	int lastDamageEvent;
	qboolean debugEnabled;
	int crashStartTime;
	int lastLoggedMode;
	qboolean targetRecoveryHandled;
	int targetRecoveryHandledNum;
	int targetRecoveryHandledEvent;
	int crashDiagNextLogTime;
	qboolean targetCrashActive;
	qboolean conserveBoost;
	botlite_tactic_t plannedTactic;
	int nextTacticThinkTime;
} botlite_runtime_state_t;

typedef struct {
	qboolean didInitialRise;
	int riseEndTime;
	int turnEndTime;
	int moveEndTime;
	float turnYawStart;
	float moveYaw;
} botlite_search_state_t;

typedef struct {
	int comboStage;
	int comboStep;
	int comboTargetHits;
	int actionUntil;
	int nextActionTime;
	int blockUntil;
	int nextBlockTime;
	int dodgeUntil;
	int nextDodgeTime;
	int dodgeDirection;
	int specialHoldUntil;
	int knockbackDirection;
	int combatState;
	int combatNextThinkTime;
	int combatStateChangedTime;
} botlite_melee_state_t;

typedef struct {
	int weapon;
	int attackMode;
	int weaponSwitchTime;
	int holdUntil;
	int approachTime;
	qboolean forceMelee;
	qboolean didSpawnFlyup;
	qboolean didInitialTransform;
	int openingStyle;
	int openingTargetNum;
} botlite_ranged_state_t;

typedef struct {
	int retreatUntil;
	int recoveryWaitEndTime;
	int recoveryWaitCooldownUntil;
	float retreatYaw;
	qboolean postCrashFlyup;
	int postCrashRiseEndTime;
	qboolean healRequested;
	qboolean healActive;
	int healLastHealth;
	int healLastCurrent;
	int healLastProgressTime;
	int knockbackStartTime;
} botlite_recovery_state_t;

typedef struct botlite_snapshot_s {
	gentity_t *bot;
	gentity_t *target;
	float distSq;
	float dist;
	float horizontalDist;
	float verticalDelta;
	qboolean hasTarget;
	qboolean reactedToDamage;
	qboolean targetCrashNow;
	qboolean targetCrashEdge;
	qboolean targetCrashPartial;
	qboolean targetStillRecovering;
	qboolean targetNeedsRecoveryWait;
	qboolean targetDead;
	qboolean hasLineOfSight;
	qboolean targetFacingBot;
	qboolean targetCharging;
	qboolean targetBlocking;
	qboolean targetInMelee;
	qboolean botInMelee;
	qboolean botFrozen;
	qboolean botDisabled;
	int botActionFlags;
	int targetWeapon;
} botlite_snapshot_t;

struct botlite_skill_vtable_s;
typedef struct botlite_skill_vtable_s botlite_skill_vtable_t;

typedef struct {
	qboolean inuse;
	int skill;
	char character[MAX_QPATH];
	char modelName[MAX_QPATH];
	char archetypeName[MAX_QPATH];
	float aggression;
	float rushTendency;
	float blockTendency;
	float specialTendency;
	float rangedBias;
	float comboCommitment;
	float targetSwitchBias;
	float powerlevelThreshold;
	float recoverRespect;
	const botlite_profile_t *profile;
	const botlite_skill_vtable_t *skillOps;
	botlite_runtime_state_t runtime;
	botlite_search_state_t search;
	botlite_melee_state_t melee;
	botlite_ranged_state_t ranged;
	botlite_recovery_state_t recovery;
	botlite_action_t action;
} botlite_info_t;

struct botlite_skill_vtable_s {
	const char *name;
	botlite_mode_t defaultMode;
	void (*ResetRuntime)( int clientNum, qboolean respawnStyle );
	void (*RunSearch)( gentity_t *bot, int clientNum, const botlite_snapshot_t *snapshot );
	botlite_tactic_t (*SelectCombatTactic)( gentity_t *bot, int clientNum, const botlite_snapshot_t *snapshot );
	void (*RunCombatTactic)( gentity_t *bot, int clientNum, const botlite_snapshot_t *snapshot, botlite_tactic_t tactic );
};

#define BOTLITE_SKILL3_OPENING_NONE 0
#define BOTLITE_SKILL3_OPENING_RANGED 1
#define BOTLITE_SKILL3_OPENING_DIRECT_MELEE 2
#define BOTLITE_SKILL3_KB_FORWARD 5
#define BOTLITE_SKILL3_KB_UP 1
#define BOTLITE_SKILL3_KB_DOWN 2

extern botlite_info_t g_botlite[BOTLITE_MAX_BOTS];

/* Profiles / skills / botsys */
void BotLite_BotsysInit( void );
qboolean BotLite_BotsysResolveBot( const char *characterName, int requestedSkill, char *outDisplayName, int displaySize, char *outModelName, int modelSize, int *outResolvedSkill, char *outArchetype, int archetypeSize );
void BotLite_BotsysApplyInfoConfig( botlite_info_t *info, const char *archetypeName );
const botlite_profile_t *BotLite_GetProfile( int skill );
const botlite_skill_vtable_t *BotLite_GetSkillVTable( int skill );
const botlite_combat_policy_t *BotLite_GetCombatPolicy( int skill );
botlite_mode_t BotLite_DefaultModeForSkill( int skill );

/* Main / lifecycle */
void BotLite_ResetAll( void );
void BotLite_OnClientDisconnect( int clientNum );
int BotLite_AddBot( const char *characterName, int skill, char *error, int errorSize );
int BotLite_AddBotDebug( const char *characterName, int skill, char *error, int errorSize );
int BotLite_RemoveBotByName( const char *characterName, char *error, int errorSize );
int BotLite_RemoveAllBots( void );
void BotLite_ResetStateForTargetDeath( int clientNum );
void BotLite_ResetStateForRespawn( int clientNum );
void BotLite_ResetStateForLostTarget( int clientNum );
void BotLite_ResetCoreRuntimeState( int clientNum, qboolean respawnStyle, qboolean clearTarget );
void BotLite_ResetTransientCombatState( int clientNum );
void BotLite_ThinkClient( int clientNum, int time );
qboolean BotLite_IsTargetDead( gentity_t *target );
qboolean BotLite_IsManagedBot( int clientNum );

int BotAISetup( int restart );
int BotAIShutdown( int restart );
int BotAILoadMap( int restart );
int BotAISetupClient( int client, struct bot_settings_s *settings, qboolean restart );
int BotAIShutdownClient( int client, qboolean restart );
int BotAIStartFrame( int time );

/* Shared helpers */
void BotLite_DebugLog( gentity_t *bot, const char *msg );
qboolean BotLite_IsTemporarilyDisabled( gentity_t *bot );
float BotLite_ShortestAngleDelta( float from, float to );
qboolean BotLite_ShouldUseBoost( gentity_t *bot, int clientNum );
qboolean BotLite_ShouldUseSanzoken( gentity_t *bot, int clientNum );
void BotLite_EA_BoostIfAllowed( gentity_t *bot, int clientNum );

/* Elementary actions */
void BotLite_ActionReset( int clientNum, int serverTime );
void BotLite_ActionCommit( gentity_t *bot, int clientNum, int serverTime );
void BotLite_EA_MoveForward( gentity_t *bot, int value );
void BotLite_EA_MoveBack( gentity_t *bot, int value );
void BotLite_EA_MoveRight( gentity_t *bot, int value );
void BotLite_EA_MoveUp( gentity_t *bot, int value );
void BotLite_EA_Button( gentity_t *bot, int buttonMask );
void BotLite_EA_SetWeapon( gentity_t *bot, int weapon );

/* Lock / orientation */
void BotLite_ClearLock( gentity_t *bot );
void BotLite_ApplyViewAngles( gentity_t *bot, const vec3_t angles );
void BotLite_FaceTarget( gentity_t *bot, gentity_t *target );
void BotLite_SetLockOn( gentity_t *bot, gentity_t *target );

/* Targeting / snapshot */
qboolean BotLite_TargetIsValid( gentity_t *bot, gentity_t *target );
gentity_t *BotLite_FindNearestVisiblePlayer( gentity_t *bot, float *outDistSq );
gentity_t *BotLite_FindNearestPlayerAnyDistance( gentity_t *bot, float *outDistSq );
gentity_t *BotLite_GetTrackedTarget( gentity_t *bot, int clientNum, float *outDistSq );
qboolean BotLite_ReactToDamage( gentity_t *bot, int clientNum, float *outDistSq );
qboolean BotLite_TargetStillRecovering( gentity_t *target );
qboolean BotLite_TargetNeedsRecoveryWait( gentity_t *target );
void BotLite_UpdateTargetRecoveryState( gentity_t *bot, int clientNum, gentity_t *target, float distSq, botlite_snapshot_t *snapshot );
void BotLite_PopulateSnapshotMetrics( gentity_t *bot, int clientNum, gentity_t *target, float distSq, botlite_snapshot_t *snapshot );

/* Movement / search / recovery */
qboolean BotLite_RunPostCrashFlyup( gentity_t *bot, int clientNum );
void BotLite_StartRecoveryWait( gentity_t *bot, int clientNum, gentity_t *target );
void BotLite_StartSkill3DeathHeal( int clientNum );
qboolean BotLite_RunRecoveryWait( gentity_t *bot, int clientNum, gentity_t *target );
void BotLite_RunSearchPattern( gentity_t *bot, int clientNum );
void BotLite_RunDefaultSearch( gentity_t *bot, int clientNum, gentity_t *target );

/* Tactics / skills */
void BotLite_RunManagedSearch( gentity_t *bot, int clientNum, const botlite_snapshot_t *snapshot );
botlite_tactic_t BotLite_SelectManagedCombatTactic( gentity_t *bot, int clientNum, const botlite_snapshot_t *snapshot );
void BotLite_RunManagedCombatTactic( gentity_t *bot, int clientNum, const botlite_snapshot_t *snapshot, botlite_tactic_t tactic );
botlite_tactic_t BotLite_SelectSkill1Tactic( gentity_t *bot, int clientNum, const botlite_snapshot_t *snapshot );
botlite_tactic_t BotLite_SelectSkill2Tactic( gentity_t *bot, int clientNum, const botlite_snapshot_t *snapshot );
void BotLite_RunSkill1Search( gentity_t *bot, int clientNum, const botlite_snapshot_t *snapshot );
void BotLite_RunSkill2Search( gentity_t *bot, int clientNum, const botlite_snapshot_t *snapshot );
void BotLite_RunSkill1Tactic( gentity_t *bot, int clientNum, const botlite_snapshot_t *snapshot, botlite_tactic_t tactic );
void BotLite_RunSkill2Tactic( gentity_t *bot, int clientNum, const botlite_snapshot_t *snapshot, botlite_tactic_t tactic );
void BotLite_RunSkill3Search( gentity_t *bot, int clientNum, const botlite_snapshot_t *snapshot );
botlite_tactic_t BotLite_SelectSkill3Tactic( gentity_t *bot, int clientNum, const botlite_snapshot_t *snapshot );
void BotLite_RunSkill3Tactic( gentity_t *bot, int clientNum, const botlite_snapshot_t *snapshot, botlite_tactic_t tactic );

/* Shared combat building blocks */
void BotLite_RunMeleePressure( gentity_t *bot, int clientNum, gentity_t *target, const botlite_snapshot_t *snapshot );
void BotLite_RunManagedMeleePressure( gentity_t *bot, int clientNum, gentity_t *target, const botlite_snapshot_t *snapshot );
void BotLite_RunSkill3MeleePressure( gentity_t *bot, int clientNum, gentity_t *target, const botlite_snapshot_t *snapshot );
qboolean BotLite_RunRangedPressure( gentity_t *bot, int clientNum, gentity_t *target, const botlite_snapshot_t *snapshot );

#endif
