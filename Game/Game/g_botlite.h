#ifndef G_BOTLITE_H
#define G_BOTLITE_H

#define BOTLITE_MAX_BOTS MAX_CLIENTS
#define BOTLITE_ACQUIRE_DISTANCE 10000.0f
#define BOTLITE_COMBAT_DISTANCE 64.0f
#define BOTLITE_MELEE_START_DISTANCE 10000.0f
#define BOTLITE_MELEE_CHASE_DISTANCE 10000.0f
#define BOTLITE_LOCK_RANGE 2200.0f
#define BOTLITE_SEARCH_RISE_TIME 3000
#define BOTLITE_SEARCH_TURN_TIME 450
#define BOTLITE_SEARCH_FORWARD_TIME 3000
#define BOTLITE_SEARCH_TURN_MIN 35.0f
#define BOTLITE_SEARCH_TURN_MAX 120.0f
#define BOTLITE_VIEW_HEIGHT 48.0f
#define BOTLITE_SKILL2_ACQUIRE_DISTANCE 1000000.0f
#define BOTLITE_SKILL2_RANGED_DISTANCE 10000.0f
#define BOTLITE_SKILL2_MELEE_EXIT_DISTANCE 10000.0f
#define BOTLITE_SKILL2_APPROACH_TRIGGER_DISTANCE 1500.0f
#define BOTLITE_SKILL2_APPROACH_INTERVAL 5000
#define BOTLITE_SKILL2_APPROACH_TIME 1200
#define BOTLITE_SKILL2_WEAPON_SWITCH_TIME 5000
#define BOTLITE_SKILL2_CHARGE_MIN 25

typedef enum {
	BOTLITE_MODE_IDLE = 0,
	BOTLITE_MODE_SEARCH,
	BOTLITE_MODE_COMBAT,
	BOTLITE_MODE_WAIT_TARGET_RECOVERY
} botlite_mode_t;

typedef enum {
	BOTLITE_COMBO_PUNCH = 0,
	BOTLITE_COMBO_KICK,
	BOTLITE_COMBO_SPEED,
	BOTLITE_COMBO_FINISH
} botlite_combo_stage_t;

typedef struct {
	qboolean inuse;
	int skill;
	char character[MAX_QPATH];
	botlite_mode_t mode;
	qboolean didInitialRise;
	int riseEndTime;
	int turnEndTime;
	int moveEndTime;
	float turnYawStart;
	float moveYaw;
	int lastTargetNum;
	int comboStage;
	int comboStep;
	int actionUntil;
	int nextActionTime;
	int lastDamageEvent;
	qboolean debugEnabled;
	int crashStartTime;
	int lastLoggedMode;
	int skill2Weapon;
	int skill2AttackMode;
	int skill2WeaponSwitchTime;
	int skill2HoldUntil;
	int skill2ApproachTime;
	qboolean skill2ForceMelee;
	int retreatUntil;
	int recoveryWaitEndTime;
	int recoveryWaitCooldownUntil;
	float retreatYaw;
	qboolean postCrashFlyup;
	int postCrashRiseEndTime;
	qboolean targetRecoveryHandled;
	int targetRecoveryHandledNum;
	int targetRecoveryHandledEvent;
	int crashDiagNextLogTime;
	qboolean targetCrashActive;
	qboolean skill2DidSpawnFlyup;
} botlite_info_t;

extern botlite_info_t g_botlite[BOTLITE_MAX_BOTS];

/* Main / lifecycle */
void BotLite_ResetAll( void );
void BotLite_OnClientDisconnect( int clientNum );
int BotLite_AddBot( const char *characterName, int skill, char *error, int errorSize );
int BotLite_AddBotDebug( const char *characterName, int skill, char *error, int errorSize );
int BotLite_RemoveBotByName( const char *characterName, char *error, int errorSize );
int BotLite_RemoveAllBots( void );

int BotAISetup( int restart );
int BotAIShutdown( int restart );
int BotAILoadMap( int restart );
int BotAISetupClient( int client, struct bot_settings_s *settings, qboolean restart );
int BotAIShutdownClient( int client, qboolean restart );
int BotAIStartFrame( int time );

/* Shared helpers */
void BotLite_DebugLog( gentity_t *bot, const char *msg );
qboolean BotLite_IsTemporarilyDisabled( gentity_t *bot );

/* Lock / orientation */
void BotLite_ClearLock( gentity_t *bot );
void BotLite_ApplyViewAngles( gentity_t *bot, const vec3_t angles );
void BotLite_FaceTarget( gentity_t *bot, gentity_t *target );
void BotLite_SetLockOn( gentity_t *bot, gentity_t *target );

/* Targeting */
qboolean BotLite_TargetIsValid( gentity_t *bot, gentity_t *target );
gentity_t *BotLite_FindNearestVisiblePlayer( gentity_t *bot, float *outDistSq );
gentity_t *BotLite_FindNearestPlayerAnyDistance( gentity_t *bot, float *outDistSq );
gentity_t *BotLite_GetTrackedTarget( gentity_t *bot, int clientNum, float *outDistSq );
qboolean BotLite_ReactToDamage( gentity_t *bot, int clientNum, float *outDistSq );
qboolean BotLite_TargetStillRecovering( gentity_t *target );
qboolean BotLite_TargetNeedsRecoveryWait( gentity_t *target );

/* Movement / search / recovery */
qboolean BotLite_RunPostCrashFlyup( gentity_t *bot, int clientNum );
void BotLite_StartRecoveryWait( gentity_t *bot, int clientNum, gentity_t *target );
qboolean BotLite_RunRecoveryWait( gentity_t *bot, int clientNum, gentity_t *target );
void BotLite_RunSearchSkill1( gentity_t *bot, int clientNum, gentity_t *target );
void BotLite_RunSearchSkill2( gentity_t *bot, int clientNum, gentity_t *target, float distSq );
void BotLite_RunSearchDefault( gentity_t *bot, int clientNum, gentity_t *target );

/* Combat */
void BotLite_RunCombatSkill1( gentity_t *bot, int clientNum, gentity_t *target, float distSq );
void BotLite_RunCombatDefault( gentity_t *bot, gentity_t *target );
void BotLite_RunCombatSkill2( gentity_t *bot, int clientNum, gentity_t *target, float distSq );

#endif
