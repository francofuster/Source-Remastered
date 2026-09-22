#ifndef G_BOTLITE_H
#define G_BOTLITE_H

#define BOTLITE_MAX_BOTS MAX_CLIENTS
#define BOTLITE_VIEW_HEIGHT 48.0f

typedef enum {
	BOTLITE_MODE_IDLE = 0,
	BOTLITE_MODE_SEARCH,
	BOTLITE_MODE_COMBAT,
	BOTLITE_MODE_WAIT_TARGET_RECOVERY,
	/* T2.7: retirarse a recuperar recursos propios (distinto de esperar al rival). */
	BOTLITE_MODE_RECOVER
} botlite_mode_t;

typedef enum {
	BOTLITE_GOAL_NONE = 0,
	BOTLITE_GOAL_SEARCH,
	BOTLITE_GOAL_COMBAT,
	BOTLITE_GOAL_WAIT_RECOVERY,
	BOTLITE_GOAL_POST_CRASH_FLYUP,
	BOTLITE_GOAL_IDLE,
	BOTLITE_GOAL_RECOVER
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
	BOTLITE_TACTIC_IDLE_TRACK,
	/* Fase 6: presion a distancia abriendo hueco a proposito. */
	BOTLITE_TACTIC_RANGED_KITE
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

typedef enum {
	BOTLITE_ENGAGE_NONE = 0,
	BOTLITE_ENGAGE_MELEE,
	BOTLITE_ENGAGE_RANGED
} botlite_engage_intent_t;

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
	/* T1.1: bloqueo disparado por la carga del rival, no por temporizador. */
	qboolean allowsReactiveBlock;
	/* T2.1: comprometerse a cargas de Power/Stun como decision, no como
		 * subproducto de los tiempos del combo. */
	qboolean allowsDeliberateCharge;
	/* T2.2: Charge Breaker reactivo contra la carga del rival. */
	qboolean allowsBreaker;
	/* T2.5: zanzoken como escape, no solo como cierre de distancia. */
	qboolean allowsDefensiveZanzoken;
	/* T2.4: cargar ki en las pausas del combate. */
	qboolean allowsKiCharge;
	/* T2.7/T2.8: decidir retirarse y ejecutar el ciclo de recuperacion. */
	qboolean allowsRecoverMode;
	/* T3.1: mezclar Evade con Block en vez de bloquear siempre. */
	qboolean allowsEvade;
	/* Fase 5: esquivar proyectiles entrantes antes de que impacten. */
	qboolean allowsDodgeIncoming;
	/* T3.4: empujar ki mas alla del 100%% cuando el rival no puede castigar. */
	qboolean allowsOffensiveBreakLimit;
	/* Fase 6: elegir melee o ranged por decision, no por distancia. */
	qboolean allowsEngageIntent;
	/* Fase 6: abrir hueco a proposito para poder disparar desde cerca. */
	qboolean allowsEngageKiting;
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
	int reactiveBlockMinReaction;
	int reactiveBlockMaxReaction;

	/* T4.1 -- tunables movidos desde #define de codigo a .cfg. */
	int staminaComfortablePct;
	int staminaTightPct;
	int staminaCriticalPct;
	float zanzokenEscapeDist;
	float kiChargeMinSafeDist;
	int kiChargeTargetPct;
	int recoverEnterPct;
	int recoverExitPct;
	/* Vida (%% de plMaximum) que tambien dispara la retirada, no solo la stamina. */
	int recoverHealthEnterPct;
	int recoverHealthExitPct;
	/* Banda de distancia del sanzoken de aproximacion. 0 = usar los umbrales
		 * hibridos ranged/melee como antes. */
	float sanzokenBandMinDist;
	float sanzokenBandMaxDist;
	int recoverMaxMs;
	float recoverSafeDist;
	int recoverKiFloorPct;
	int recoverKiFloorHiddenPct;
	int breakLimitMinKiPct;
	int breakLimitHoldMs;
	int struggleCommitPct;
	int breakerTapMs;
	int chargeCommitCooldown;
	int stunChargeHoldMs;

	/* Fase 6 -- pesos y tiempos del score de intencion de enganche. */
	int engageSwitchMinMs;
	float engageSwitchMargin;
	float engageWeightBias;
	float engageWeightKi;
	float engageWeightStamina;
	float engageWeightHealth;
	float engageWeightTargetState;
	float engageWeightDistance;
	float engageWeightEffect;
	float engageNeutralDist;
	float engageKiteMinDist;

	/* Fase 6.1 -- etapa de recarga del ciclo de recuperacion. */
	int recoverRestExitPct;
	int recoverRechargeMinStaminaPct;
	int recoverRechargeKiPct;
	int recoverCooldownMs;
	int transformIntervalMs;
	int tierEscalateMarginPct;
	int tierEscalateCombatMs;
	/* Fase 6.6: umbral de "estado critico" que habilita escapar. */
	int escapeCriticalHealthPct;

	/* Fase 6.3 -- llegar al melee en horizontal. */
	float meleeApproachMaxPitch;
	float meleeApproachLevelTolerance;
	float meleeApproachSteepRatio;
} botlite_profile_t;

typedef struct {
	int forwardmove;
	int rightmove;
	int upmove;
	int buttons;
	int weapon;
	qboolean weaponOverride;
	/* T0.2: el boost es lo unico que suma potencia en un forcejeo de haces, y
	 * normalmente se descarta al atacar. Esta excepcion lo deja pasar. */
	qboolean allowBoostWithAttack;
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
	/* T3.2: lectura de patron. Persiste entre exchanges (no se resetea en cada
		 * limpieza de estado transitorio), solo al cambiar de objetivo o morir. */
	float targetSpeedBreakerBias;
	int patternTargetNum;
	qboolean speedBreakerEdgeSeen;
	int snapshotDiagNextLogTime;
	int struggleStartTime;
	/* Fase 6.5: el boost no dejaba rastro en el log, y ya hubo dos reportes de
	 * "no hace boost" sin nada que mirar. */
	int boostDenyLogTime;
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
	/* T1.1: un unico veredicto por carga del rival; sin esto la tirada se
	 * repetiria cada frame y el bot bloquearia siempre. */
	qboolean reactiveChargeSeen;
	qboolean reactiveBlockCommitted;
	int reactiveBlockReadyTime;
	int reactiveBlockUntil;
	qboolean reactiveDefenseIsEvade;
	int chargeCommitUntil;
	int chargeCommitButton;
	int chargeNextCommitTime;
	int breakerHoldUntil;
	int breakerNextTime;
	qboolean breakerEpisodeUsed;
	qboolean targetChargingNow;
	int zanzokenEscapeNextTime;
	int dodgeIncomingNextTime;
	/* Fase 6.2 -- zanzoken sostenido. El motor exige mantener el boton Y una
	 * direccion de movimiento durante toda la duracion; un toque de un frame
	 * paga el costo entero y no desplaza nada. */
	int zanzokenHoldUntil;
	int zanzokenMoveForward;
	int zanzokenMoveRight;
	int zanzokenMoveUp;
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
	/* T3.3: punish a distancia tras un Power melee conectado. */
	qboolean punishKnockbackDecided;
	qboolean punishKnockbackUseRanged;
	/* Fase 6.4 -- limite de reintentos de transformacion. */
	int tierNextAttemptTime;
	int tierLastLogTime;
	int tierEngagedSince;
	/* Fase 6.7: el boton de transformacion tambien hay que sostenerlo. */
	int tierHoldUntil;
	int tierHoldDirection;
	int tierLastSeen;
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
	/* T2.7/T2.8 -- ciclo de recuperacion */
	qboolean recoverActive;
	int recoverStage;
	int recoverEndTime;
	int breakLimitUntil;
	qboolean breakLimitDecided;
	qboolean kiChargeActive;
	/* Fase 6.1 -- etapa RECHARGE: cuando empezo la etapa actual, y seguimiento
	 * de si la vida realmente esta subiendo (si no sube, no hay pool que gastar). */
	int recoverStageTime;
	int recoverLastHealth;
	int recoverHealthProgressTime;
	/* Enfriamiento entre episodios: sin esto, un timeout reentra al frame siguiente. */
	int recoverNextAllowedTime;
} botlite_recovery_state_t;

/* Fase 6 -- intencion de enganche: la decision de pelear de cerca o de lejos,
 * y la medicion de que tan bien esta funcionando cada una. */
typedef struct {
	botlite_engage_intent_t intent;
	int nextSwitchTime;
	float lastScore;
	int lastLogTime;
	/* Ventana en la que el bot abre hueco a proposito antes de disparar. */
	int kiteUntil;
	/* Media movil de efectividad neta por intencion (dano hecho - recibido). */
	float meleeEffect;
	float rangedEffect;
	int sampleTargetNum;
	int sampleTargetHealth;
	int sampleBotHealth;
	int sampleTime;
} botlite_engage_state_t;

/* Presupuesto de recursos (T0.3) */
typedef enum {
	BOTLITE_STAMINA_COMFORTABLE = 0,
	BOTLITE_STAMINA_TIGHT,
	BOTLITE_STAMINA_CRITICAL,
	BOTLITE_STAMINA_DRAINED
} botlite_stamina_t;

typedef enum {
	BOTLITE_SPEND_CHEAP = 0,	/* boost puntual */
	BOTLITE_SPEND_NORMAL,		/* carga de melee, ataque de ki */
	BOTLITE_SPEND_EXPENSIVE	/* zanzoken, transformacion */
} botlite_spend_t;

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
	/* --- T0.1: estado de melee del rival y recursos de ambos ---
	 * stMeleeState es el dato que resuelve el duelo de melee (Speed/Power/Stun/
	 * Block/Evade/Breaker). Sin esto el bot no puede reaccionar, solo adivinar. */
	int botMeleeState;
	int targetMeleeState;
	int botMeleeChargeTime;
	int targetMeleeChargeTime;
	int botKnockbackTime;
	int botBoostTime;
	int targetKnockbackTime;
	qboolean botStruggling;
	qboolean targetStruggling;
	int botFatigue;
	int botFatigueMax;
	int botKi;
	int botKiMax;
	int botHealth;
	int botTier;
	int targetFatigue;
	int targetKi;
	int targetKiMax;
	int targetHealth;
	int targetTier;
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
	botlite_engage_state_t engage;
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
void BotLite_EA_AllowBoostWithAttack( gentity_t *bot );

/* Recursos (T0.3) */
qboolean BotLite_RealStaminaEnabled( void );
int BotLite_StaminaPercent( int clientNum );
int BotLite_KiPercent( int clientNum );
int BotLite_HealthPercent( int clientNum );
botlite_stamina_t BotLite_StaminaBudget( int clientNum );
const char *BotLite_StaminaBudgetName( botlite_stamina_t budget );
qboolean BotLite_StaminaAllowsSpend( int clientNum, botlite_spend_t cost );
qboolean BotLite_InCriticalState( gentity_t *bot, int clientNum );
void BotLite_SyncStaminaMode( gentity_t *bot );

/* Forcejeo de haces (T0.2) */
qboolean BotLite_IsStruggling( gentity_t *bot );
qboolean BotLite_RunStruggle( gentity_t *bot, int clientNum, const botlite_snapshot_t *snapshot );
void BotLite_StrugglePrepare( gentity_t *bot, int clientNum );

/* Lock / orientation */
void BotLite_ClearLock( gentity_t *bot );
void BotLite_ApplyViewAngles( gentity_t *bot, const vec3_t angles );
void BotLite_FaceTarget( gentity_t *bot, gentity_t *target );
void BotLite_FaceTargetLeveled( gentity_t *bot, gentity_t *target, float maxPitch );
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
const char *BotLite_MeleeStateName( int meleeState );
void BotLite_DebugLogSnapshot( gentity_t *bot, int clientNum, const botlite_snapshot_t *snapshot );

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
void BotLite_ApproachTargetLeveled( gentity_t *bot, int clientNum, const botlite_snapshot_t *snapshot );
qboolean BotLite_TargetIsChargingMelee( const botlite_snapshot_t *snapshot );
qboolean BotLite_RunDeliberateCharge( gentity_t *bot, int clientNum, const botlite_snapshot_t *snapshot );
qboolean BotLite_RunReactiveBreaker( gentity_t *bot, int clientNum, const botlite_snapshot_t *snapshot );

/* Transformacion (T2.3) */
qboolean BotLite_RunTransformControl( gentity_t *bot, int clientNum, const botlite_snapshot_t *snapshot );

/* Escape y recursos (T2.4 / T2.5) */
qboolean BotLite_RunDefensiveZanzoken( gentity_t *bot, int clientNum, const botlite_snapshot_t *snapshot );

/* Fase 6 -- alternancia melee/ranged por decision */
void BotLite_UpdateEngageIntent( gentity_t *bot, int clientNum, const botlite_snapshot_t *snapshot );
botlite_engage_intent_t BotLite_GetEngageIntent( int clientNum );
qboolean BotLite_EngageIntentActive( int clientNum );
qboolean BotLite_EngageWantsKite( int clientNum, const botlite_snapshot_t *snapshot );
qboolean BotLite_RunRangedKite( gentity_t *bot, int clientNum, gentity_t *target, const botlite_snapshot_t *snapshot );
void BotLite_ResetEngageState( int clientNum, qboolean fullReset );
const char *BotLite_EngageIntentName( botlite_engage_intent_t intent );

/* Fase 5 -- esquive de ataques entrantes */
qboolean BotLite_RunDodgeIncoming( gentity_t *bot, int clientNum, const botlite_snapshot_t *snapshot );

/* Fase 6.2 -- zanzoken sostenido (compartido por escape, esquive y aproximacion) */
int BotLite_ZanzokenDurationMs( gentity_t *bot );
void BotLite_StartZanzoken( gentity_t *bot, int clientNum, int moveForward, int moveRight, int moveUp );
qboolean BotLite_RunZanzokenHold( gentity_t *bot, int clientNum, const botlite_snapshot_t *snapshot );
qboolean BotLite_RunKiCharge( gentity_t *bot, int clientNum, const botlite_snapshot_t *snapshot );

/* Desenganche y recuperacion (T2.7 / T2.8) */
qboolean BotLite_UpdateRecoverMode( gentity_t *bot, int clientNum, const botlite_snapshot_t *snapshot );
qboolean BotLite_RunRecoverCycle( gentity_t *bot, int clientNum, const botlite_snapshot_t *snapshot );

/* T3.4 */
qboolean BotLite_RunOffensiveBreakLimit( gentity_t *bot, int clientNum, const botlite_snapshot_t *snapshot );
void BotLite_RunSkill3MeleePressure( gentity_t *bot, int clientNum, gentity_t *target, const botlite_snapshot_t *snapshot );
qboolean BotLite_RunRangedPressure( gentity_t *bot, int clientNum, gentity_t *target, const botlite_snapshot_t *snapshot );

#endif
