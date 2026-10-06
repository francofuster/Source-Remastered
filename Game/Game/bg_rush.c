/*
===========================================================================
bg_rush.c -- Combate Rush, strike-by-strike melee (both games)

Replaces PM_Melee when the server sets PSO_RUSH_COMBAT (cvar g_rushCombat).

Every press of the attack button is one strike taken from the speed melee
poses S1..S6 (frames 338-361, 4 frames per pose). The defender answers with
the reaction pose of the same number from SPEED_MELEE_HIT / BLOCK / DODGE,
which were modelled for that strike. The six POWER_MELEE moves are the
directional finishers and charged smashes.

The attacker writes the result of a hit straight into the playerState of the
locked opponent, like PM_Melee did. That only happens on the server: the
client does not predict while locked on (cg_predict.c), so the cgame never
runs the attacker side.

Timings are in ms and are checked as thresholds crossed, not as windows, so
the 50 ms steps of a locked-on server frame never skip a hit.

NOTE: no apostrophes in comments, the q3cpp of the QVM toolchain breaks.
===========================================================================
*/
#include "../../Shared/q_shared.h"
#include "bg_public.h"
#include "bg_local.h"

int		trap_FS_FOpenFile( const char *qpath, fileHandle_t *f, fsMode_t mode );
void	trap_FS_Read( void *buffer, int len, fileHandle_t f );
void	trap_FS_FCloseFile( fileHandle_t f );

bgRushConfig_t	bg_rush;

static int		pm_rushEvents;		// rush events queued in this pmove step

/*
==============================================================================

MOVE TABLE

==============================================================================
*/

static int BG_RushFindMove( const char *name ) {
	int i;
	for ( i = 1 ; i <= bg_rush.numMoves ; i++ ) {
		if ( !Q_stricmp( bg_rush.moves[i].name, name ) ) {
			return i;
		}
	}
	return 0;
}

static int BG_RushAddMove( const char *name ) {
	int				index;
	bgRushMove_t	*m;
	index = BG_RushFindMove( name );
	if ( index ) {
		return index;
	}
	if ( bg_rush.numMoves >= RUSH_MAX_MOVES - 1 ) {
		Com_Printf( "^3Rush: too many moves, %s ignored\n", name );
		return 0;
	}
	index = ++bg_rush.numMoves;
	m = &bg_rush.moves[index];
	memset( m, 0, sizeof( *m ) );
	Q_strncpyz( m->name, name, sizeof( m->name ) );
	m->startup = 80;
	m->active = 40;
	m->recovery = 200;
	m->range = 40;
	m->damage = 0.04f;
	m->hitstun = 320;
	m->blockstun = 200;
	m->launchSpeed = 1.0f;
	m->launchTime = 1200;
	return index;
}

static int BG_RushDefStrike( const char *name, int strike, int react, int startup, int active, int recovery,
							int range, float damage, int hitstun, int blockstun, int push, float cost, int sound ) {
	int				index;
	bgRushMove_t	*m;
	index = BG_RushAddMove( name );
	m = &bg_rush.moves[index];
	m->strike = strike;
	m->react = react;
	m->startup = startup;
	m->active = active;
	m->recovery = recovery;
	m->range = range;
	m->damage = damage;
	m->hitstun = hitstun;
	m->blockstun = blockstun;
	m->push = push;
	m->cost = cost;
	m->sound = sound;
	return index;
}

static int BG_RushDefPower( const char *name, int power, int startup, int recovery, int range, float damage,
							int launch, float launchSpeed, int launchTime, int stun, float cost, int sound ) {
	int				index;
	bgRushMove_t	*m;
	index = BG_RushAddMove( name );
	m = &bg_rush.moves[index];
	m->strike = 0;
	m->anim = ANIM_POWER_MELEE_1_HIT + power - 1;
	m->react = 0;
	m->startup = startup;
	m->active = 60;
	m->recovery = recovery;
	m->range = range;
	m->damage = damage;
	m->hitstun = 400;
	m->blockstun = 340;
	m->launch = launch;
	m->launchSpeed = launchSpeed;
	m->launchTime = launchTime;
	m->stun = stun;
	m->cost = cost;
	m->sound = sound;
	return index;
}

void BG_RushDefaults( void ) {
	memset( &bg_rush, 0, sizeof( bg_rush ) );

	// Strikes: S1..S6 poses. Range is the distance measured on the models at
	// which the pose touches the paired reaction pose.
	//                name        S  R  start act  rec  range dmg    hitst blkst push cost    sound
	BG_RushDefStrike( "jab",      1, 1,  70,  40, 150,  45, 0.040f, 320,  200,   0, 0.004f, RSND_JAB );
	BG_RushDefStrike( "cross",    3, 3,  60,  40, 140,  35, 0.040f, 320,  200,   0, 0.004f, RSND_JAB2 );
	BG_RushDefStrike( "knee",     2, 2,  60,  40, 160,  22, 0.045f, 340,  210,   0, 0.004f, RSND_KICK );
	BG_RushDefStrike( "uppercut", 4, 4,  80,  40, 180,  21, 0.045f, 360,  220,   0, 0.005f, RSND_JAB );
	BG_RushDefStrike( "kickMid",  5, 5,  90,  50, 200,  49, 0.050f, 420,  240, 450, 0.005f, RSND_KICK2 );
	BG_RushDefStrike( "kickLow",  6, 6,  90,  50, 200,  53, 0.050f, 400,  240,   0, 0.005f, RSND_KICK );

	// Finishers: POWER_MELEE 1..6, fast, ending the chain.
	//               name              P  start rec  range dmg    launch speed  time  stun cost   sound
	BG_RushDefPower( "heavy",          1, 140, 380,  45, 0.080f, 0, 1.0f,    0,  700, 0.03f, RSND_HEAVY );
	BG_RushDefPower( "lift",           4, 140, 380,  30, 0.100f, 1, 1.0f, 1200,    0, 0.03f, RSND_POWER5 );
	BG_RushDefPower( "slam",           2, 140, 380,  30, 0.100f, 2, 1.2f, 1000,    0, 0.03f, RSND_POWER6 );
	BG_RushDefPower( "sweepLeft",      5, 140, 380,  35, 0.100f, 4, 1.0f, 1200,    0, 0.03f, RSND_POWER2 );
	BG_RushDefPower( "sweepRight",     6, 140, 380,  35, 0.100f, 3, 1.0f, 1200,    0, 0.03f, RSND_POWER2 );
	BG_RushDefPower( "dropkick",       3, 160, 400,  50, 0.100f, 5, 1.5f, 1600,    0, 0.04f, RSND_HEAVY );
	BG_RushDefPower( "launchStraight", 1, 140, 380,  45, 0.090f, 5, 1.0f, 1400,    0, 0.03f, RSND_HEAVY );
	// Revenge counter: comes out at once from hitstun, pushes the attacker off.
	BG_RushDefPower( "revenge",        1,   0, 300,  60, 0.030f, 5, 0.8f,  600,    0, 0.00f, RSND_HEAVY );

	// Smashes: same poses, charged. Level 1..3 scales damage, speed and time.
	BG_RushDefPower( "smashStraight",  1,  80, 300,  45, 0.100f, 5, 1.0f, 1500,    0, 0.05f, RSND_HEAVY );
	BG_RushDefPower( "smashUp",        4,  80, 300,  30, 0.100f, 1, 1.0f, 1500,    0, 0.05f, RSND_POWER5 );
	BG_RushDefPower( "smashDown",      2,  80, 300,  30, 0.100f, 2, 1.2f, 1200,    0, 0.05f, RSND_POWER6 );
	BG_RushDefPower( "smashLeft",      5,  80, 300,  35, 0.100f, 4, 1.0f, 1500,    0, 0.05f, RSND_POWER2 );
	BG_RushDefPower( "smashRight",     6,  80, 300,  35, 0.100f, 3, 1.0f, 1500,    0, 0.05f, RSND_POWER2 );
	BG_RushDefPower( "smashBoost",     3, 100, 320,  50, 0.110f, 5, 1.4f, 2000,    0, 0.06f, RSND_HEAVY );

	bg_rush.chain[0][0] = BG_RushFindMove( "jab" );
	bg_rush.chain[0][1] = BG_RushFindMove( "cross" );
	bg_rush.chain[0][2] = BG_RushFindMove( "knee" );
	bg_rush.chain[0][3] = BG_RushFindMove( "uppercut" );
	bg_rush.chain[0][4] = BG_RushFindMove( "kickMid" );
	bg_rush.chainLen[0] = 5;
	bg_rush.chain[1][0] = BG_RushFindMove( "jab" );
	bg_rush.chain[1][1] = BG_RushFindMove( "kickMid" );
	bg_rush.chain[1][2] = BG_RushFindMove( "kickLow" );
	bg_rush.chain[1][3] = BG_RushFindMove( "knee" );
	bg_rush.chain[1][4] = BG_RushFindMove( "kickLow" );
	bg_rush.chainLen[1] = 5;

	bg_rush.finisher[RDIR_NEUTRAL] = BG_RushFindMove( "heavy" );
	bg_rush.finisher[RDIR_UP] = BG_RushFindMove( "lift" );
	bg_rush.finisher[RDIR_DOWN] = BG_RushFindMove( "slam" );
	bg_rush.finisher[RDIR_LEFT] = BG_RushFindMove( "sweepLeft" );
	bg_rush.finisher[RDIR_RIGHT] = BG_RushFindMove( "sweepRight" );
	bg_rush.finisher[RDIR_BOOST] = BG_RushFindMove( "dropkick" );
	bg_rush.finisherRepeat = BG_RushFindMove( "launchStraight" );
	bg_rush.revenge = BG_RushFindMove( "revenge" );

	bg_rush.smash[RDIR_NEUTRAL] = BG_RushFindMove( "smashStraight" );
	bg_rush.smash[RDIR_UP] = BG_RushFindMove( "smashUp" );
	bg_rush.smash[RDIR_DOWN] = BG_RushFindMove( "smashDown" );
	bg_rush.smash[RDIR_LEFT] = BG_RushFindMove( "smashLeft" );
	bg_rush.smash[RDIR_RIGHT] = BG_RushFindMove( "smashRight" );
	bg_rush.smash[RDIR_BOOST] = BG_RushFindMove( "smashBoost" );

	bg_rush.engageRange = 120;
	bg_rush.minSpacing = 32;
	bg_rush.reachSlack = 24;
	bg_rush.magnetSpeed = 2500;
	bg_rush.comboStep = 0.10f;
	bg_rush.comboMin = 0.30f;
	bg_rush.finisherMin = 0.50f;
	bg_rush.lifeHealth = 0.70f;
	bg_rush.lifeMax = 0.50f;
	bg_rush.guardChip = 0.10f;
	bg_rush.guardCost = 0.015f;
	bg_rush.guardBreakAt = 0.10f;
	bg_rush.guardBreakStun = 800;
	bg_rush.smashLevel2 = 550;
	bg_rush.smashLevel3 = 900;
	bg_rush.smashMax = 1200;
	bg_rush.smashDamage[0] = 1.0f;
	bg_rush.smashDamage[1] = 1.4f;
	bg_rush.smashDamage[2] = 1.8f;
	bg_rush.smashSpeed[0] = 1.0f;
	bg_rush.smashSpeed[1] = 1.25f;
	bg_rush.smashSpeed[2] = 1.5f;
	bg_rush.smashTime[0] = 1.0f;
	bg_rush.smashTime[1] = 1.5f;
	bg_rush.smashTime[2] = 2.0f;
	bg_rush.launchMinSpeed = 900;
	bg_rush.launchRecover = 600;
	bg_rush.chainEndSpeed = 0.5f;
	bg_rush.chainEndTime = 500;

	bg_rush.parryWindow = 100;
	bg_rush.parryRearm = 500;
	bg_rush.parryStagger = 400;
	bg_rush.swayWindow = 120;
	bg_rush.swayCost = 0.05f;
	bg_rush.swayTime = 220;
	bg_rush.swayPunish = 200;
	bg_rush.vanishWindow = 150;
	bg_rush.vanishWindowStep = 30;
	bg_rush.vanishWindowMin = 60;
	bg_rush.vanishCost = 0.04f;
	bg_rush.vanishCostStep = 0.02f;
	bg_rush.vanishDistance = 45;
	bg_rush.vanishPunish = 200;
	bg_rush.backDamage = 1.25f;
	bg_rush.counterWindow = 100;
	bg_rush.counterDamage = 1.5f;
	bg_rush.revengeCost = 0.20f;

	bg_rush.rushInRange = 600;
	bg_rush.rushInSpeed = 2400;
	bg_rush.rushInMax = 700;
	bg_rush.rushInCost = 0.03f;
	bg_rush.assaultWindow = 300;
	bg_rush.assaultRange = 500;
	bg_rush.assaultCost = 0.10f;
	bg_rush.assaultExposed = 300;
	bg_rush.chaseRange = 2500;
	bg_rush.chaseLead = 45;
	bg_rush.chaseWindow = 450;
	bg_rush.chaseMax = 3;
	bg_rush.chaseCost[0] = 0.08f;
	bg_rush.chaseCost[1] = 0.12f;
	bg_rush.chaseCost[2] = 0.16f;

	bg_rush.clashWindow = 50;
	bg_rush.clashTime = 500;

	bg_rush.wallStun = 900;
	bg_rush.crashRecover = 800;
}

/*
==============================================================================

CONFIG FILE

players/rushDefault.cfg, optional. Blocks:
	settings { key value ... }
	move <name> { key value ... }		adds a move or overrides a built-in one
	chain default|side { move move ... }
	finisher <dir> <move>				dir: neutral up down left right boost repeat
	smash <dir> <move>

==============================================================================
*/

static char	bg_rushText[16384];

static int BG_RushParseDir( const char *token ) {
	if ( !Q_stricmp( token, "up" ) ) { return RDIR_UP; }
	if ( !Q_stricmp( token, "down" ) ) { return RDIR_DOWN; }
	if ( !Q_stricmp( token, "left" ) ) { return RDIR_LEFT; }
	if ( !Q_stricmp( token, "right" ) ) { return RDIR_RIGHT; }
	if ( !Q_stricmp( token, "boost" ) ) { return RDIR_BOOST; }
	if ( !Q_stricmp( token, "repeat" ) ) { return RDIR_COUNT; }
	if ( !Q_stricmp( token, "revenge" ) ) { return RDIR_COUNT + 1; }
	return RDIR_NEUTRAL;
}

static int BG_RushParseLaunch( const char *token ) {
	if ( !Q_stricmp( token, "up" ) ) { return 1; }
	if ( !Q_stricmp( token, "down" ) ) { return 2; }
	if ( !Q_stricmp( token, "right" ) ) { return 3; }
	if ( !Q_stricmp( token, "left" ) ) { return 4; }
	if ( !Q_stricmp( token, "away" ) ) { return 5; }
	return 0;
}

static int BG_RushParseSound( const char *token ) {
	if ( !Q_stricmp( token, "jab" ) ) { return RSND_JAB; }
	if ( !Q_stricmp( token, "jab2" ) ) { return RSND_JAB2; }
	if ( !Q_stricmp( token, "kick" ) ) { return RSND_KICK; }
	if ( !Q_stricmp( token, "kick2" ) ) { return RSND_KICK2; }
	if ( !Q_stricmp( token, "heavy" ) ) { return RSND_HEAVY; }
	if ( !Q_stricmp( token, "power2" ) ) { return RSND_POWER2; }
	if ( !Q_stricmp( token, "power5" ) ) { return RSND_POWER5; }
	if ( !Q_stricmp( token, "power6" ) ) { return RSND_POWER6; }
	return RSND_NONE;
}

// "strike1".."strike6" or "power1".."power6"
static void BG_RushParseAnim( bgRushMove_t *m, const char *token ) {
	int n;
	n = atoi( token + strlen( token ) - 1 );
	if ( n < 1 || n > 6 ) {
		Com_Printf( "^3Rush: bad anim %s in move %s\n", token, m->name );
		return;
	}
	if ( !Q_stricmpn( token, "strike", 6 ) ) {
		m->strike = n;
		m->anim = 0;
	}
	else if ( !Q_stricmpn( token, "power", 5 ) ) {
		m->strike = 0;
		m->anim = ANIM_POWER_MELEE_1_HIT + n - 1;
	}
	else {
		Com_Printf( "^3Rush: bad anim %s in move %s\n", token, m->name );
	}
}

static qboolean BG_RushExpectBrace( char **p ) {
	char *token;
	token = COM_Parse( p );
	if ( Q_stricmp( token, "{" ) ) {
		Com_Printf( "^3Rush: expected { but found %s\n", token );
		return qfalse;
	}
	return qtrue;
}

static void BG_RushParseSettings( char **p ) {
	char	*token;
	char	key[64];
	if ( !BG_RushExpectBrace( p ) ) {
		return;
	}
	while ( 1 ) {
		token = COM_Parse( p );
		if ( !token[0] || !Q_stricmp( token, "}" ) ) {
			return;
		}
		Q_strncpyz( key, token, sizeof( key ) );
		token = COM_Parse( p );
		if ( !Q_stricmp( key, "engageRange" ) ) { bg_rush.engageRange = atoi( token ); }
		else if ( !Q_stricmp( key, "minSpacing" ) ) { bg_rush.minSpacing = atoi( token ); }
		else if ( !Q_stricmp( key, "reachSlack" ) ) { bg_rush.reachSlack = atoi( token ); }
		else if ( !Q_stricmp( key, "magnetSpeed" ) ) { bg_rush.magnetSpeed = atoi( token ); }
		else if ( !Q_stricmp( key, "comboStep" ) ) { bg_rush.comboStep = atof( token ); }
		else if ( !Q_stricmp( key, "comboMin" ) ) { bg_rush.comboMin = atof( token ); }
		else if ( !Q_stricmp( key, "finisherMin" ) ) { bg_rush.finisherMin = atof( token ); }
		else if ( !Q_stricmp( key, "lifeHealth" ) ) { bg_rush.lifeHealth = atof( token ); }
		else if ( !Q_stricmp( key, "lifeMax" ) ) { bg_rush.lifeMax = atof( token ); }
		else if ( !Q_stricmp( key, "guardChip" ) ) { bg_rush.guardChip = atof( token ); }
		else if ( !Q_stricmp( key, "guardCost" ) ) { bg_rush.guardCost = atof( token ); }
		else if ( !Q_stricmp( key, "guardBreakAt" ) ) { bg_rush.guardBreakAt = atof( token ); }
		else if ( !Q_stricmp( key, "guardBreakStun" ) ) { bg_rush.guardBreakStun = atoi( token ); }
		else if ( !Q_stricmp( key, "smashLevel2" ) ) { bg_rush.smashLevel2 = atoi( token ); }
		else if ( !Q_stricmp( key, "smashLevel3" ) ) { bg_rush.smashLevel3 = atoi( token ); }
		else if ( !Q_stricmp( key, "smashMax" ) ) { bg_rush.smashMax = atoi( token ); }
		else if ( !Q_stricmp( key, "smashDamage2" ) ) { bg_rush.smashDamage[1] = atof( token ); }
		else if ( !Q_stricmp( key, "smashDamage3" ) ) { bg_rush.smashDamage[2] = atof( token ); }
		else if ( !Q_stricmp( key, "smashSpeed2" ) ) { bg_rush.smashSpeed[1] = atof( token ); }
		else if ( !Q_stricmp( key, "smashSpeed3" ) ) { bg_rush.smashSpeed[2] = atof( token ); }
		else if ( !Q_stricmp( key, "smashTime2" ) ) { bg_rush.smashTime[1] = atof( token ); }
		else if ( !Q_stricmp( key, "smashTime3" ) ) { bg_rush.smashTime[2] = atof( token ); }
		else if ( !Q_stricmp( key, "launchMinSpeed" ) ) { bg_rush.launchMinSpeed = atoi( token ); }
		else if ( !Q_stricmp( key, "launchRecover" ) ) { bg_rush.launchRecover = atoi( token ); }
		else if ( !Q_stricmp( key, "chainEndSpeed" ) ) { bg_rush.chainEndSpeed = atof( token ); }
		else if ( !Q_stricmp( key, "chainEndTime" ) ) { bg_rush.chainEndTime = atoi( token ); }
		else if ( !Q_stricmp( key, "parryWindow" ) ) { bg_rush.parryWindow = atoi( token ); }
		else if ( !Q_stricmp( key, "parryRearm" ) ) { bg_rush.parryRearm = atoi( token ); }
		else if ( !Q_stricmp( key, "parryStagger" ) ) { bg_rush.parryStagger = atoi( token ); }
		else if ( !Q_stricmp( key, "swayWindow" ) ) { bg_rush.swayWindow = atoi( token ); }
		else if ( !Q_stricmp( key, "swayCost" ) ) { bg_rush.swayCost = atof( token ); }
		else if ( !Q_stricmp( key, "swayTime" ) ) { bg_rush.swayTime = atoi( token ); }
		else if ( !Q_stricmp( key, "swayPunish" ) ) { bg_rush.swayPunish = atoi( token ); }
		else if ( !Q_stricmp( key, "vanishWindow" ) ) { bg_rush.vanishWindow = atoi( token ); }
		else if ( !Q_stricmp( key, "vanishWindowStep" ) ) { bg_rush.vanishWindowStep = atoi( token ); }
		else if ( !Q_stricmp( key, "vanishWindowMin" ) ) { bg_rush.vanishWindowMin = atoi( token ); }
		else if ( !Q_stricmp( key, "vanishCost" ) ) { bg_rush.vanishCost = atof( token ); }
		else if ( !Q_stricmp( key, "vanishCostStep" ) ) { bg_rush.vanishCostStep = atof( token ); }
		else if ( !Q_stricmp( key, "vanishDistance" ) ) { bg_rush.vanishDistance = atoi( token ); }
		else if ( !Q_stricmp( key, "vanishPunish" ) ) { bg_rush.vanishPunish = atoi( token ); }
		else if ( !Q_stricmp( key, "backDamage" ) ) { bg_rush.backDamage = atof( token ); }
		else if ( !Q_stricmp( key, "counterWindow" ) ) { bg_rush.counterWindow = atoi( token ); }
		else if ( !Q_stricmp( key, "counterDamage" ) ) { bg_rush.counterDamage = atof( token ); }
		else if ( !Q_stricmp( key, "revengeCost" ) ) { bg_rush.revengeCost = atof( token ); }
		else if ( !Q_stricmp( key, "rushInRange" ) ) { bg_rush.rushInRange = atoi( token ); }
		else if ( !Q_stricmp( key, "rushInSpeed" ) ) { bg_rush.rushInSpeed = atoi( token ); }
		else if ( !Q_stricmp( key, "rushInMax" ) ) { bg_rush.rushInMax = atoi( token ); }
		else if ( !Q_stricmp( key, "rushInCost" ) ) { bg_rush.rushInCost = atof( token ); }
		else if ( !Q_stricmp( key, "assaultWindow" ) ) { bg_rush.assaultWindow = atoi( token ); }
		else if ( !Q_stricmp( key, "assaultRange" ) ) { bg_rush.assaultRange = atoi( token ); }
		else if ( !Q_stricmp( key, "assaultCost" ) ) { bg_rush.assaultCost = atof( token ); }
		else if ( !Q_stricmp( key, "assaultExposed" ) ) { bg_rush.assaultExposed = atoi( token ); }
		else if ( !Q_stricmp( key, "chaseRange" ) ) { bg_rush.chaseRange = atoi( token ); }
		else if ( !Q_stricmp( key, "chaseLead" ) ) { bg_rush.chaseLead = atoi( token ); }
		else if ( !Q_stricmp( key, "chaseWindow" ) ) { bg_rush.chaseWindow = atoi( token ); }
		else if ( !Q_stricmp( key, "chaseMax" ) ) { bg_rush.chaseMax = atoi( token ); }
		else if ( !Q_stricmp( key, "chaseCost1" ) ) { bg_rush.chaseCost[0] = atof( token ); }
		else if ( !Q_stricmp( key, "chaseCost2" ) ) { bg_rush.chaseCost[1] = atof( token ); }
		else if ( !Q_stricmp( key, "chaseCost3" ) ) { bg_rush.chaseCost[2] = atof( token ); }
		else if ( !Q_stricmp( key, "clashWindow" ) ) { bg_rush.clashWindow = atoi( token ); }
		else if ( !Q_stricmp( key, "clashTime" ) ) { bg_rush.clashTime = atoi( token ); }
		else if ( !Q_stricmp( key, "wallStun" ) ) { bg_rush.wallStun = atoi( token ); }
		else if ( !Q_stricmp( key, "crashRecover" ) ) { bg_rush.crashRecover = atoi( token ); }
		else { Com_Printf( "^3Rush: unknown setting %s\n", key ); }
	}
}

static void BG_RushParseMove( char **p ) {
	char			*token;
	char			key[64];
	int				index;
	bgRushMove_t	*m;
	token = COM_Parse( p );
	index = BG_RushAddMove( token );
	if ( !BG_RushExpectBrace( p ) ) {
		return;
	}
	m = index ? &bg_rush.moves[index] : NULL;
	while ( 1 ) {
		token = COM_Parse( p );
		if ( !token[0] || !Q_stricmp( token, "}" ) ) {
			return;
		}
		Q_strncpyz( key, token, sizeof( key ) );
		token = COM_Parse( p );
		if ( !m ) {
			continue;
		}
		if ( !Q_stricmp( key, "anim" ) ) { BG_RushParseAnim( m, token ); }
		else if ( !Q_stricmp( key, "react" ) ) { m->react = atoi( token ); }
		else if ( !Q_stricmp( key, "startup" ) ) { m->startup = atoi( token ); }
		else if ( !Q_stricmp( key, "active" ) ) { m->active = atoi( token ); }
		else if ( !Q_stricmp( key, "recovery" ) ) { m->recovery = atoi( token ); }
		else if ( !Q_stricmp( key, "range" ) ) { m->range = atoi( token ); }
		else if ( !Q_stricmp( key, "damage" ) ) { m->damage = atof( token ); }
		else if ( !Q_stricmp( key, "hitstun" ) ) { m->hitstun = atoi( token ); }
		else if ( !Q_stricmp( key, "blockstun" ) ) { m->blockstun = atoi( token ); }
		else if ( !Q_stricmp( key, "push" ) ) { m->push = atoi( token ); }
		else if ( !Q_stricmp( key, "launch" ) ) { m->launch = BG_RushParseLaunch( token ); }
		else if ( !Q_stricmp( key, "launchSpeed" ) ) { m->launchSpeed = atof( token ); }
		else if ( !Q_stricmp( key, "launchTime" ) ) { m->launchTime = atoi( token ); }
		else if ( !Q_stricmp( key, "stun" ) ) { m->stun = atoi( token ); }
		else if ( !Q_stricmp( key, "cost" ) ) { m->cost = atof( token ); }
		else if ( !Q_stricmp( key, "sound" ) ) { m->sound = BG_RushParseSound( token ); }
		else if ( !Q_stricmp( key, "guardBreak" ) ) { m->guardBreak = atoi( token ); }
		else { Com_Printf( "^3Rush: unknown key %s in move %s\n", key, m->name ); }
	}
}

static void BG_RushParseChain( char **p ) {
	char	*token;
	int		which, count, index;
	token = COM_Parse( p );
	which = !Q_stricmp( token, "side" ) ? 1 : 0;
	if ( !BG_RushExpectBrace( p ) ) {
		return;
	}
	count = 0;
	while ( 1 ) {
		token = COM_Parse( p );
		if ( !token[0] || !Q_stricmp( token, "}" ) ) {
			break;
		}
		index = BG_RushFindMove( token );
		if ( !index ) {
			Com_Printf( "^3Rush: chain uses unknown move %s\n", token );
			continue;
		}
		if ( count < RUSH_CHAIN_MAX ) {
			bg_rush.chain[which][count++] = index;
		}
	}
	if ( count ) {
		bg_rush.chainLen[which] = count;
	}
}

static void BG_RushParseSlot( char **p, qboolean smash ) {
	char	*token;
	int		dir, index;
	token = COM_Parse( p );
	dir = BG_RushParseDir( token );
	token = COM_Parse( p );
	index = BG_RushFindMove( token );
	if ( !index ) {
		Com_Printf( "^3Rush: unknown move %s\n", token );
		return;
	}
	if ( dir >= RDIR_COUNT ) {
		if ( !smash && dir == RDIR_COUNT ) {
			bg_rush.finisherRepeat = index;
		}
		else if ( !smash ) {
			bg_rush.revenge = index;
		}
		return;
	}
	if ( smash ) {
		bg_rush.smash[dir] = index;
	}
	else {
		bg_rush.finisher[dir] = index;
	}
}

void BG_RushLoadConfig( const char *path ) {
	fileHandle_t	f;
	int				len;
	char			*p, *token;

	BG_RushDefaults();
	len = trap_FS_FOpenFile( path, &f, FS_READ );
	if ( len <= 0 ) {
		Com_Printf( "Rush: %s not found, using built-in moves\n", path );
		return;
	}
	if ( len >= (int)sizeof( bg_rushText ) ) {
		Com_Printf( "^3Rush: %s is too long, using built-in moves\n", path );
		trap_FS_FCloseFile( f );
		return;
	}
	trap_FS_Read( bg_rushText, len, f );
	bg_rushText[len] = 0;
	trap_FS_FCloseFile( f );

	p = bg_rushText;
	while ( 1 ) {
		token = COM_Parse( &p );
		if ( !token[0] ) {
			break;
		}
		if ( !Q_stricmp( token, "settings" ) ) { BG_RushParseSettings( &p ); }
		else if ( !Q_stricmp( token, "move" ) ) { BG_RushParseMove( &p ); }
		else if ( !Q_stricmp( token, "chain" ) ) { BG_RushParseChain( &p ); }
		else if ( !Q_stricmp( token, "finisher" ) ) { BG_RushParseSlot( &p, qfalse ); }
		else if ( !Q_stricmp( token, "smash" ) ) { BG_RushParseSlot( &p, qtrue ); }
		else { Com_Printf( "^3Rush: unknown block %s in %s\n", token, path ); }
	}
	Com_Printf( "Rush: %s loaded, %i moves\n", path, bg_rush.numMoves );
}

/*
==============================================================================

ANIMATION HELPERS (shared with the cgame)

==============================================================================
*/

int BG_RushAnim( int type, int pose, int variant ) {
	if ( pose < 1 ) { pose = 1; }
	if ( pose > 6 ) { pose = 6; }
	return ANIM_RUSH_FIRST + ( variant ? RUSH_ANIM_SET : 0 ) + type * 6 + ( pose - 1 );
}

qboolean BG_RushIsRushAnim( int anim ) {
	anim &= ~ANIM_TOGGLEBIT;
	return ( anim >= ANIM_RUSH_FIRST && anim <= ANIM_RUSH_LAST ) ? qtrue : qfalse;
}

// Time the cgame takes to blend into a strike pose: the startup of the
// fastest move that uses it, so the pose lands when the hit is checked.
int BG_RushStrikeLerp( int pose ) {
	int i, best;
	best = 0;
	for ( i = 1 ; i <= bg_rush.numMoves ; i++ ) {
		if ( bg_rush.moves[i].strike == pose && ( !best || bg_rush.moves[i].startup < best ) ) {
			best = bg_rush.moves[i].startup;
		}
	}
	if ( !best ) { best = 70; }
	if ( best < 30 ) { best = 30; }
	if ( best > 200 ) { best = 200; }
	return best;
}

// Health bars (g_healthBars): the health gauge is split in this many bars
// and every damage is divided by it, so a fight lasts that many times longer.
int BG_HealthBars( const playerState_t *ps ) {
	int bars;
	bars = ps->stats[stHealthBars];
	if ( bars < 1 ) { bars = 1; }
	if ( bars > 3 ) { bars = 3; }	// green, magenta and red: no fourth color stays clear of the HUD ones
	return bars;
}

const char *BG_RushStateName( int state ) {
	switch ( state ) {
	case RS_NONE:		return "idle";
	case RS_STARTUP:	return "startup";
	case RS_ACTIVE:		return "active";
	case RS_RECOVERY:	return "recovery";
	case RS_CHARGE:		return "charge";
	case RS_RUSHIN:		return "rush-in";
	case RS_CHASE:		return "chase";
	case RS_CLASH:		return "clash";
	case RS_DODGE:		return "dodge";
	case RS_HITSTUN:	return "hitstun";
	case RS_BLOCKSTUN:	return "blockstun";
	case RS_STUNNED:	return "stunned";
	}
	return "?";
}

/*
==============================================================================

PMOVE

==============================================================================
*/

#define RH_FORCE	1	// no guard and no defensive answer (clash winner, revenge, counter)
#define RH_COUNTER	2	// super counter damage
#define RH_BACK		4	// from behind: cannot be guarded, more damage

#define RUSH_CHASES( ps )	( ( ps )->stats[stRushCount] & 15 )
#define RUSH_DUELS( ps )	( ( ( ps )->stats[stRushCount] >> 4 ) & 15 )

qboolean PM_RushEnabled( void ) {
	return ( pm->ps->options & PSO_RUSH_COMBAT ) ? qtrue : qfalse;
}

qboolean PM_RushBusy( void ) {
	if ( !PM_RushEnabled() ) {
		return qfalse;
	}
	return pm->ps->stats[stRushState] != RS_NONE ? qtrue : qfalse;
}

static void PM_RushDebug( const char *text ) {
	if ( pm->ps->options & PSO_RUSH_DEBUG ) {
		Com_Printf( "[rush %6i] cl%i %s\n", pm->ps->commandTime, pm->ps->clientNum, text );
	}
}

// Events go on the player who causes them. A pmove step can only carry
// MAX_PS_EVENTS events of its own player before they overwrite each other.
static void BG_RushEvent( playerState_t *ps, int newEvent, int parm ) {
	if ( ps == pm->ps ) {
		if ( pm_rushEvents >= MAX_PS_EVENTS ) {
			return;
		}
		pm_rushEvents++;
	}
	BG_AddPredictableEventToPlayerstate( newEvent, parm, ps );
}

static void PM_RushEvent( int newEvent, int parm ) {
	BG_RushEvent( pm->ps, newEvent, parm );
}

// Plays an animation on legs and torso of any player (the attacker also
// drives the reaction of the defender).
static void BG_RushPlay( playerState_t *ps, int anim ) {
	ps->legsAnim = ( ( ps->legsAnim & ANIM_TOGGLEBIT ) ^ ANIM_TOGGLEBIT ) | anim;
	ps->torsoAnim = ( ( ps->torsoAnim & ANIM_TOGGLEBIT ) ^ ANIM_TOGGLEBIT ) | anim;
	ps->legsTimer = 0;
	ps->torsoTimer = 0;
}

// Rush poses alternate between two copies so the same pose twice in a row
// still restarts on the client (ANIM_TOGGLEBIT does not survive the 8 bit
// legsAnim / torsoAnim fields of the network code).
static void BG_RushPlayPose( playerState_t *ps, int type, int pose ) {
	ps->stats[stRushFlags] ^= RF_VARIANT;
	BG_RushPlay( ps, BG_RushAnim( type, pose, ( ps->stats[stRushFlags] & RF_VARIANT ) ? 1 : 0 ) );
}

// Back to the stance the footsteps code would pick, so the strike pose does
// not linger for a frame on the legs while the torso already went idle.
static void BG_RushNeutralAnim( playerState_t *ps ) {
	if ( ps->lockedTarget > 0 ) {
		BG_RushPlay( ps, ANIM_IDLE_LOCKED );
	}
	else if ( ps->bitFlags & usingFlight ) {
		BG_RushPlay( ps, ANIM_FLY_IDLE );
	}
	else {
		BG_RushPlay( ps, ANIM_IDLE );
	}
}

static void BG_RushClearMove( playerState_t *ps ) {
	ps->stats[stRushMove] = 0;
	ps->stats[stRushChain] = 0;
	ps->stats[stRushFlags] &= ~( RF_CONNECTED | RF_RESOLVED | RF_BUF_ATK | RF_BUF_ALT | RF_FINISHER | RF_SIDE_CHAIN
		| RF_COUNTER | RF_CHASED | RF_CLASH_ANY );
	ps->timers[tmRushTime] = 0;
}

static void BG_RushSetCount( playerState_t *ps, int chases, int duels ) {
	if ( chases > 15 ) { chases = 15; }
	if ( duels > 15 ) { duels = 15; }
	ps->stats[stRushCount] = chases | ( duels << 4 );
}

static qboolean BG_RushUntouchable( playerState_t *foe ) {
	if ( foe->bitFlags & ( isDead | isUnconcious | isCrashed | usingZanzoken | isStruggling | isTransforming ) ) {
		return qtrue;
	}
	if ( foe->timers[tmKnockback] || foe->timers[tmTransform] > 0 ) {
		return qtrue;
	}
	if ( foe->persistant[PERS_TEAM] == TEAM_SPECTATOR ) {
		return qtrue;
	}
	if ( foe->stats[stRushState] == RS_DODGE ) {
		return qtrue;
	}
	return qfalse;
}

// A chase strike may also hit a defender who is still flying from a launch.
static qboolean PM_RushCanHit( playerState_t *foe ) {
	if ( !BG_RushUntouchable( foe ) ) {
		return qtrue;
	}
	if ( ( pm->ps->stats[stRushFlags] & RF_CHASED ) && foe->timers[tmKnockback] > 0
		&& !( foe->bitFlags & ( isDead | isUnconcious | isCrashed | isStruggling | isTransforming ) )
		&& foe->stats[stRushState] != RS_DODGE ) {
		return qtrue;
	}
	return qfalse;
}

static playerState_t *PM_RushFoe( void ) {
	playerState_t *foe;
	if ( pm->ps->lockedTarget <= 0 || !pm->ps->lockedPlayer || !pm->ps->lockedPosition ) {
		return NULL;
	}
	foe = pm->ps->lockedPlayer;
	// the pointer is refreshed by EV_MELEE_CHECK, reject it while stale
	if ( foe->clientNum != pm->ps->lockedTarget - 1 || foe == pm->ps ) {
		return NULL;
	}
	return foe;
}

// true when "from" stands behind the way "ps" is facing
static qboolean BG_RushBehind( playerState_t *ps, const vec3_t from ) {
	vec3_t angles, forward, dir;
	VectorSet( angles, 0, ps->viewangles[YAW], 0 );
	AngleVectors( angles, forward, NULL, NULL );
	VectorSubtract( from, ps->origin, dir );
	dir[2] = 0;
	if ( VectorNormalize( dir ) < 1.0f ) {
		return qfalse;
	}
	return DotProduct( forward, dir ) < -0.3f ? qtrue : qfalse;
}

static void BG_RushFace( playerState_t *ps, const vec3_t target ) {
	vec3_t dir;
	VectorSubtract( target, ps->origin, dir );
	vectoangles( dir, ps->viewangles );
}

// Puts ps at around + offset. Fails when a wall leaves no room, so the two
// players never end up inside each other.
static qboolean PM_RushTeleport( playerState_t *ps, const vec3_t around, const vec3_t offset, int passEntityNum ) {
	trace_t	trace;
	vec3_t	end;
	VectorAdd( around, offset, end );
	pm->trace( &trace, around, pm->mins, pm->maxs, end, passEntityNum, MASK_PLAYERSOLID & ~CONTENTS_BODY );
	if ( trace.allsolid || trace.startsolid || trace.fraction < 0.75f ) {
		return qfalse;
	}
	VectorCopy( trace.endpos, ps->origin );
	VectorClear( ps->velocity );
	ps->eFlags ^= EF_TELEPORT_BIT;
	return qtrue;
}

static int PM_RushDir( void ) {
	if ( pm->cmd.upmove > 0 || ( pm->cmd.buttons & BUTTON_JUMP ) ) { return RDIR_UP; }
	if ( pm->cmd.upmove < 0 ) { return RDIR_DOWN; }
	if ( pm->cmd.rightmove > 0 ) { return RDIR_RIGHT; }
	if ( pm->cmd.rightmove < 0 ) { return RDIR_LEFT; }
	if ( ( pm->cmd.buttons & BUTTON_BOOST ) || ( pm->ps->bitFlags & usingBoost ) ) { return RDIR_BOOST; }
	return RDIR_NEUTRAL;
}

static int BG_RushSmashLevel( playerState_t *ps ) {
	int level;
	level = ps->stats[stRushChain] - RUSH_SMASH_BASE;
	return ( level >= 1 && level <= 3 ) ? level : 0;
}

static qboolean BG_RushPay( playerState_t *ps, float share ) {
	int cost;
	cost = ps->powerLevel[plMaximum] * share;
	if ( cost > 0 && ps->powerLevel[plFatigue] < cost ) {
		return qfalse;
	}
	ps->powerLevel[plUseFatigue] += cost;
	return qtrue;
}

/*
==============================================================================
Input tracking: runs every pmove step, also while flying from a knockback,
so the defensive presses (needed for the vanish duel) are never missed.
==============================================================================
*/
void PM_RushTrackInput( void ) {
	playerState_t	*ps;
	int				*flags;
	qboolean		blkPress, telePress;

	ps = pm->ps;
	flags = &ps->stats[stRushFlags];
	pml.rushAtkPress = ( ( pm->cmd.buttons & BUTTON_ATTACK ) && !( *flags & RF_ATK_HELD ) ) ? qtrue : qfalse;
	pml.rushAltPress = ( ( pm->cmd.buttons & BUTTON_ALT_ATTACK ) && !( *flags & RF_ALT_HELD ) ) ? qtrue : qfalse;
	pml.rushAltRelease = ( !( pm->cmd.buttons & BUTTON_ALT_ATTACK ) && ( *flags & RF_ALT_HELD ) ) ? qtrue : qfalse;
	blkPress = ( ( pm->cmd.buttons & BUTTON_BLOCK ) && !( *flags & RF_BLK_HELD ) ) ? qtrue : qfalse;
	telePress = ( ( pm->cmd.buttons & BUTTON_TELEPORT ) && !( *flags & RF_TELE_HELD ) ) ? qtrue : qfalse;
	pml.rushBlkPress = blkPress;
	pml.rushTelePress = telePress;

	*flags &= ~( RF_ATK_HELD | RF_ALT_HELD | RF_BLK_HELD | RF_TELE_HELD );
	if ( pm->cmd.buttons & BUTTON_ATTACK ) { *flags |= RF_ATK_HELD; }
	if ( pm->cmd.buttons & BUTTON_ALT_ATTACK ) { *flags |= RF_ALT_HELD; }
	if ( pm->cmd.buttons & BUTTON_BLOCK ) { *flags |= RF_BLK_HELD; }
	if ( pm->cmd.buttons & BUTTON_TELEPORT ) { *flags |= RF_TELE_HELD; }

	if ( ps->timers[tmRushExposed] > 0 ) {
		ps->timers[tmRushExposed] -= pml.msec;
		if ( ps->timers[tmRushExposed] < 0 ) { ps->timers[tmRushExposed] = 0; }
	}

	// Defensive intent: the last guard or zanzoken press decides what the
	// next incoming hit meets (parry, sway or vanish), if it comes in time.
	// Mashing does not help: a new press only counts after parryRearm.
	if ( ps->timers[tmRushGuard] > 0 ) {
		ps->timers[tmRushGuard] += pml.msec;
		if ( ps->timers[tmRushGuard] > 5000 ) { ps->timers[tmRushGuard] = 5000; }
	}
	if ( blkPress || telePress ) {
		if ( ps->timers[tmRushGuard] <= 0 || ps->timers[tmRushGuard] >= bg_rush.parryRearm ) {
			*flags &= ~RF_INT_ANY;
			if ( telePress ) {
				*flags |= RF_INT_VANISH;
			}
			else if ( pm->cmd.rightmove || pm->cmd.forwardmove < 0 ) {
				*flags |= RF_INT_SWAY;
			}
			else {
				*flags |= RF_INT_PARRY;
			}
			ps->timers[tmRushGuard] = 1;
		}
	}
}

// A launched defender can be chased: the zanzoken press is the chase, not a dash.
qboolean PM_RushChaseAvailable( void ) {
	playerState_t *foe;
	if ( !PM_RushEnabled() ) {
		return qfalse;
	}
	foe = PM_RushFoe();
	if ( !foe || foe->timers[tmKnockback] <= 0 || ( foe->bitFlags & ( isCrashed | isDead | isUnconcious ) ) ) {
		return qfalse;
	}
	return ( RUSH_CHASES( pm->ps ) < bg_rush.chaseMax && Distance( pm->ps->origin, foe->origin ) <= bg_rush.chaseRange ) ? qtrue : qfalse;
}

qboolean PM_RushExposed( void ) {
	return ( PM_RushEnabled() && pm->ps->timers[tmRushExposed] > 0 ) ? qtrue : qfalse;
}

/*
==============================================================================
Starting moves
==============================================================================
*/

static void PM_RushStartMove( int index, int chainStep, int flags ) {
	playerState_t	*ps;
	bgRushMove_t	*m;
	ps = pm->ps;
	if ( index <= 0 || index > bg_rush.numMoves ) {
		return;
	}
	m = &bg_rush.moves[index];
	BG_RushClearMove( ps );
	ps->stats[stRushMove] = index;
	ps->stats[stRushState] = RS_STARTUP;
	ps->stats[stRushChain] = chainStep;
	ps->stats[stRushFlags] |= flags;
	if ( m->strike ) {
		BG_RushPlayPose( ps, RUSH_ANIM_STRIKE, m->strike );
	}
	else {
		BG_RushPlay( ps, m->anim );
	}
	ps->powerLevel[plUseFatigue] += ps->powerLevel[plMaximum] * m->cost;
	PM_RushEvent( EV_RUSH_SWING, index );
	PM_RushDebug( va( "start %s step %i", m->name, chainStep ) );
}

static void PM_RushStartChain( int step, int flags ) {
	int which;
	if ( step <= 1 ) {
		pm->ps->stats[stRushFlags] &= ~RF_SIDE_CHAIN;
		if ( pm->cmd.rightmove ) {
			pm->ps->stats[stRushFlags] |= RF_SIDE_CHAIN;
		}
		step = 1;
	}
	which = ( pm->ps->stats[stRushFlags] & RF_SIDE_CHAIN ) ? 1 : 0;
	if ( step > bg_rush.chainLen[which] ) {
		return;
	}
	pm->ps->stats[stRushStep] = step;
	PM_RushStartMove( bg_rush.chain[which][step - 1], step, flags | ( which ? RF_SIDE_CHAIN : 0 ) );
}

// A press from neutral. While the defender is still reeling from this combo
// the chain goes on from where it was, so stopping and pressing again cannot
// loop the first hits forever: the chain always reaches its last hit, which
// knocks the defender away. Only the heavy finish (crumple) restarts it.
// Forward held on the press makes it a super counter candidate.
static void PM_RushStartFresh( playerState_t *foe ) {
	int step, which, flags;
	step = 1;
	flags = pm->cmd.forwardmove > 0 ? RF_COUNTER : 0;
	if ( foe && foe->stats[stRushState] == RS_HITSTUN && foe->stats[stRushCombo] > 0 && pm->ps->stats[stRushStep] > 0 ) {
		pm->ps->stats[stRushFlags] &= ~RF_SIDE_CHAIN;
		if ( pm->cmd.rightmove ) {
			pm->ps->stats[stRushFlags] |= RF_SIDE_CHAIN;
		}
		which = ( pm->ps->stats[stRushFlags] & RF_SIDE_CHAIN ) ? 1 : 0;
		step = pm->ps->stats[stRushStep] + 1;
		if ( step > bg_rush.chainLen[which] ) {
			step = bg_rush.chainLen[which];
		}
		flags = 0;
	}
	PM_RushStartChain( step, flags );
}

static int PM_RushFinisherIndex( int dir ) {
	int index;
	index = bg_rush.finisher[dir];
	if ( dir == RDIR_NEUTRAL && ( pm->ps->stats[stRushFlags] & RF_HEAVY_USED ) && bg_rush.finisherRepeat ) {
		index = bg_rush.finisherRepeat;
	}
	return index;
}

static void PM_RushStartFinisher( void ) {
	PM_RushStartMove( PM_RushFinisherIndex( PM_RushDir() ), pm->ps->stats[stRushChain],
		RF_FINISHER | ( pm->ps->stats[stRushFlags] & RF_SIDE_CHAIN ) );
}

static void PM_RushStartSmash( void ) {
	int charge, level;
	charge = pm->ps->timers[tmRushTime];
	level = charge >= bg_rush.smashLevel3 ? 3 : ( charge >= bg_rush.smashLevel2 ? 2 : 1 );
	PM_RushStartMove( bg_rush.smash[PM_RushDir()], RUSH_SMASH_BASE + level, RF_FINISHER );
	PM_RushDebug( va( "smash level %i after %i ms", level, charge ) );
}

// A move that lands at once (counter, clash winner, revenge): skip startup.
static void BG_RushRunResolved( playerState_t *ps, int index, int flags ) {
	bgRushMove_t *m;
	m = &bg_rush.moves[index];
	BG_RushClearMove( ps );
	ps->stats[stRushMove] = index;
	ps->stats[stRushState] = RS_ACTIVE;
	ps->stats[stRushFlags] |= flags | RF_RESOLVED | RF_CONNECTED;
	ps->timers[tmRushTime] = m->startup;
	if ( m->strike ) {
		BG_RushPlayPose( ps, RUSH_ANIM_STRIKE, m->strike );
	}
	else {
		BG_RushPlay( ps, m->anim );
	}
}

/*
==============================================================================
Reactions written into another player
==============================================================================
*/

static void BG_RushInterrupt( playerState_t *ps ) {
	BG_RushClearMove( ps );
	ps->stats[stRushState] = RS_NONE;
	ps->timers[tmRushStun] = 0;
}

static void BG_RushStun( playerState_t *ps, int state, int ms, int anim, int type, int pose ) {
	BG_RushInterrupt( ps );
	ps->stats[stRushState] = state;
	ps->timers[tmRushStun] = ms;
	if ( pose ) {
		BG_RushPlayPose( ps, type, pose );
	}
	else {
		BG_RushPlay( ps, anim );
	}
	ps->weaponstate = WEAPON_READY;
	ps->stats[stChargePercentPrimary] = 0;
	ps->stats[stChargePercentSecondary] = 0;
	ps->bitFlags &= ~usingZanzoken;
	ps->timers[tmZanzoken] = 0;
}

static void BG_RushLaunch( playerState_t *att, playerState_t *def, bgRushMove_t *m, int level ) {
	float	speed;
	int		time;
	speed = ( att->powerLevel[plCurrent] / 21.84f ) * m->launchSpeed;
	time = m->launchTime;
	if ( level ) {
		speed *= bg_rush.smashSpeed[level - 1];
		time *= bg_rush.smashTime[level - 1];
	}
	if ( speed < bg_rush.launchMinSpeed * m->launchSpeed ) {
		speed = bg_rush.launchMinSpeed * m->launchSpeed;
	}
	BG_RushInterrupt( def );
	def->timers[tmRushStun] = bg_rush.launchRecover < time ? bg_rush.launchRecover : time;	// read by PM_CheckKnockback
	def->powerups[PW_KNOCKBACK_SPEED] = speed;
	def->knockBackDirection = m->launch;
	def->timers[tmKnockback] = time;
	def->weaponstate = WEAPON_READY;
	def->bitFlags &= ~usingZanzoken;
	def->timers[tmZanzoken] = 0;
	// PM_CheckKnockback reads the directions from the view of the defender:
	// face the attacker so "away", "left" and "right" are the ones the
	// attacker sees.
	BG_RushFace( def, att->origin );
}

static void BG_RushAutoLock( playerState_t *def, playerState_t *att ) {
	if ( def->lockedTarget > 0 ) {
		return;
	}
	def->lockedTarget = att->clientNum + 1;
	def->lockedPlayer = att;
	def->lockedPosition = &att->origin;
}

// Damage, guard, launch, crumple or hitstun of move "index" from att on def.
static void BG_RushApplyHit( playerState_t *att, playerState_t *def, int index, int how ) {
	bgRushMove_t	*m;
	int				level, defState, damage, cost;
	float			scale, base;
	qboolean		guarding, breaker, finisher;
	vec3_t			push;

	m = &bg_rush.moves[index];
	level = BG_RushSmashLevel( att );
	finisher = ( att->stats[stRushFlags] & RF_FINISHER ) ? qtrue : qfalse;
	defState = def->stats[stRushState];
	guarding = ( !( how & ( RH_FORCE | RH_BACK ) ) && ( def->bitFlags & usingBlock )
		&& ( defState == RS_NONE || defState == RS_BLOCKSTUN ) ) ? qtrue : qfalse;
	breaker = ( m->guardBreak || level >= 3 ) ? qtrue : qfalse;

	// a new combo starts when the defender is not already reeling
	if ( def->stats[stRushCombo] == 0 ) {
		att->stats[stRushHits] = 0;
		att->stats[stRushFlags] &= ~RF_HEAVY_USED;
		BG_RushSetCount( att, 0, RUSH_DUELS( att ) );
	}
	scale = 1.0f - bg_rush.comboStep * def->stats[stRushCombo];
	if ( scale < ( finisher ? bg_rush.finisherMin : bg_rush.comboMin ) ) {
		scale = finisher ? bg_rush.finisherMin : bg_rush.comboMin;
	}
	base = att->powerLevel[plCurrent] * m->damage * ( att->stats[stMeleeAttack] > 0 ? att->stats[stMeleeAttack] : 1 );
	if ( level ) {
		base *= bg_rush.smashDamage[level - 1];
	}
	if ( how & RH_COUNTER ) {
		base *= bg_rush.counterDamage;
	}
	if ( how & RH_BACK ) {
		base *= bg_rush.backDamage;
	}
	damage = base * scale;
	if ( damage < 1 ) {
		damage = 1;
	}
	att->stats[stRushFlags] |= RF_CONNECTED;
	BG_RushAutoLock( def, att );
	// a hit that lands or is guarded ends any vanish duel
	BG_RushSetCount( att, RUSH_CHASES( att ), 0 );
	BG_RushSetCount( def, RUSH_CHASES( def ), 0 );

	if ( guarding && !breaker ) {
		def->powerLevel[plDamageFromMelee] += damage * bg_rush.guardChip;
		cost = def->powerLevel[plMaximum] * bg_rush.guardCost * ( finisher ? 2 : 1 );
		def->powerLevel[plUseFatigue] += cost;
		if ( def->powerLevel[plFatigue] - cost < def->powerLevel[plMaximum] * bg_rush.guardBreakAt ) {
			BG_RushStun( def, RS_STUNNED, bg_rush.guardBreakStun, ANIM_STUNNED, 0, 0 );
			BG_RushEvent( att, EV_RUSH_GUARDBREAK, index );
			PM_RushDebug( va( "%s breaks the guard (fatigue)", m->name ) );
			return;
		}
		BG_RushStun( def, RS_BLOCKSTUN, m->blockstun, ANIM_BLOCK, RUSH_ANIM_BLOCK, m->react );
		BG_RushEvent( att, EV_RUSH_BLOCK, index );
		PM_RushDebug( va( "%s guarded, chip %i", m->name, (int)( damage * bg_rush.guardChip ) ) );
		return;
	}

	// Strikes cannot finish a fight, like the old speed melee: below the
	// damage the defender loses fatigue instead. Finishers and smashes can.
	// Health bars divide the damage that reaches the health (PM_BurnPowerLevel),
	// so the check and the lifesteal use that same share.
	if ( !finisher && def->powerLevel[plHealth] <= damage / BG_HealthBars( def ) ) {
		def->powerLevel[plUseFatigue] += damage;
	}
	else {
		def->powerLevel[plDamageFromMelee] += damage;
		att->powerLevel[plHealthPool] += damage * bg_rush.lifeHealth / BG_HealthBars( def );
		att->powerLevel[plMaximumPool] += damage * bg_rush.lifeMax / BG_HealthBars( def );
	}
	def->stats[stRushCombo]++;
	att->stats[stRushHits]++;

	if ( guarding && breaker ) {
		BG_RushStun( def, RS_STUNNED, bg_rush.guardBreakStun, ANIM_STUNNED, 0, 0 );
		BG_RushEvent( att, EV_RUSH_GUARDBREAK, index );
		PM_RushDebug( va( "%s breaks the guard, damage %i", m->name, damage ) );
		return;
	}
	if ( m->launch ) {
		BG_RushLaunch( att, def, m, level );
		BG_RushEvent( att, EV_RUSH_LAUNCH, index );
		PM_RushDebug( va( "%s launches, damage %i, combo %i", m->name, damage, def->stats[stRushCombo] ) );
		return;
	}
	if ( m->stun ) {
		BG_RushStun( def, RS_STUNNED, m->stun, ANIM_STUNNED_MELEE, 0, 0 );
		att->stats[stRushFlags] |= RF_HEAVY_USED;
		BG_RushEvent( att, EV_RUSH_STUN, index );
		PM_RushDebug( va( "%s crumples, damage %i, combo %i", m->name, damage, def->stats[stRushCombo] ) );
		return;
	}
	if ( !finisher && att->stats[stRushChain] >= bg_rush.chainLen[( att->stats[stRushFlags] & RF_SIDE_CHAIN ) ? 1 : 0] ) {
		bgRushMove_t ender;
		ender = *m;
		ender.launch = 5;
		ender.launchSpeed = bg_rush.chainEndSpeed;
		ender.launchTime = bg_rush.chainEndTime;
		BG_RushLaunch( att, def, &ender, 0 );
		BG_RushEvent( att, ( how & RH_COUNTER ) ? EV_RUSH_COUNTER : EV_RUSH_HIT, index );
		PM_RushDebug( va( "%s ends the chain, damage %i, combo %i", m->name, damage, def->stats[stRushCombo] ) );
		return;
	}
	BG_RushStun( def, RS_HITSTUN, m->hitstun, ANIM_STUNNED, RUSH_ANIM_HIT, m->react );
	VectorClear( def->velocity );
	if ( m->push ) {
		VectorSubtract( def->origin, att->origin, push );
		VectorNormalize( push );
		VectorScale( push, m->push, def->velocity );
	}
	BG_RushEvent( att, ( how & RH_COUNTER ) ? EV_RUSH_COUNTER : EV_RUSH_HIT, index );
	PM_RushDebug( va( "%s %s, damage %i, combo %i", m->name, ( how & RH_COUNTER ) ? "COUNTER" : ( ( how & RH_BACK ) ? "hits the back" : "hits" ),
		damage, def->stats[stRushCombo] ) );
}

/*
==============================================================================
Defensive answers, checked when the hit of the attacker (pm->ps) lands
==============================================================================
*/

static void PM_RushWhiff( bgRushMove_t *m, const char *why ) {
	PM_RushEvent( EV_RUSH_WHIFF, pm->ps->stats[stRushMove] );
	PM_RushDebug( va( "%s whiff (%s)", m->name, why ) );
}

// Zanzoken pressed in time: the defender reappears behind the attacker, who
// swings at nothing and stays turned away. Every exchange in a row shortens
// the window and costs more, until one of the two fails (vanish duel).
static qboolean PM_RushTryVanish( playerState_t *foe, bgRushMove_t *m, float dist, int reach ) {
	playerState_t	*ps;
	int				duels, window;
	qboolean		flying;
	vec3_t			angles, forward, offset;

	ps = pm->ps;
	if ( !( foe->stats[stRushFlags] & RF_INT_VANISH ) || foe->timers[tmRushGuard] <= 0 ) {
		return qfalse;
	}
	duels = RUSH_DUELS( foe );
	window = bg_rush.vanishWindow - duels * bg_rush.vanishWindowStep;
	if ( window < bg_rush.vanishWindowMin ) {
		window = bg_rush.vanishWindowMin;
	}
	if ( foe->timers[tmRushGuard] > window ) {
		return qfalse;
	}
	flying = foe->timers[tmKnockback] > 0 ? qtrue : qfalse;
	if ( flying && !( ps->stats[stRushFlags] & RF_CHASED ) ) {
		return qfalse;
	}
	// from neutral, from a guard, or from a move that already swung and
	// missed (that is what keeps a vanish duel going)
	if ( !flying && foe->stats[stRushState] != RS_NONE && foe->stats[stRushState] != RS_BLOCKSTUN
		&& foe->stats[stRushState] != RS_RECOVERY
		&& !( foe->stats[stRushState] == RS_ACTIVE && ( foe->stats[stRushFlags] & RF_RESOLVED ) ) ) {
		return qfalse;
	}
	if ( foe->bitFlags & ( isDead | isUnconcious | isCrashed | isStruggling | isTransforming ) ) {
		return qfalse;
	}
	if ( dist > reach * 3 ) {
		return qfalse;		// already dashed far away: a plain miss
	}
	if ( foe->powerLevel[plFatigue] < foe->powerLevel[plMaximum] * ( bg_rush.vanishCost + bg_rush.vanishCostStep * duels ) ) {
		return qfalse;
	}
	VectorSet( angles, 0, ps->viewangles[YAW], 0 );
	AngleVectors( angles, forward, NULL, NULL );
	VectorScale( forward, -bg_rush.vanishDistance, offset );
	if ( !PM_RushTeleport( foe, ps->origin, offset, ps->clientNum ) ) {
		return qfalse;		// a wall right behind the attacker
	}
	BG_RushPay( foe, bg_rush.vanishCost + bg_rush.vanishCostStep * duels );
	BG_RushFace( foe, ps->origin );
	foe->timers[tmKnockback] = 0;
	foe->knockBackDirection = 0;
	foe->bitFlags &= ~usingZanzoken;
	foe->timers[tmZanzoken] = 0;
	BG_RushInterrupt( foe );
	BG_RushNeutralAnim( foe );
	foe->stats[stRushFlags] &= ~RF_INT_ANY;
	foe->timers[tmRushGuard] = 0;
	BG_RushAutoLock( foe, ps );
	BG_RushSetCount( foe, RUSH_CHASES( foe ), duels + 1 );
	BG_RushSetCount( ps, RUSH_CHASES( ps ), duels + 1 );

	ps->timers[tmRushTime] -= bg_rush.vanishPunish;
	ps->timers[tmRushExposed] = m->active + m->recovery + bg_rush.vanishPunish;
	BG_RushEvent( foe, EV_RUSH_VANISH, ps->stats[stRushMove] );
	PM_RushEvent( EV_RUSH_WHIFF, ps->stats[stRushMove] );
	PM_RushDebug( va( "%s vanished behind (exchange %i)", m->name, duels + 1 ) );
	return qtrue;
}

// Guard plus a direction in time: the paired dodge pose, no damage, and the
// attacker is left with a longer recovery to punish.
static void PM_RushSway( playerState_t *foe, bgRushMove_t *m ) {
	BG_RushStun( foe, RS_DODGE, bg_rush.swayTime, ANIM_SPEED_MELEE_DODGE, RUSH_ANIM_DODGE, m->react ? m->react : 1 );
	foe->stats[stRushFlags] &= ~RF_INT_ANY;
	foe->timers[tmRushGuard] = 0;
	BG_RushAutoLock( foe, pm->ps );
	pm->ps->timers[tmRushTime] -= bg_rush.swayPunish;
	PM_RushEvent( EV_RUSH_DODGE, pm->ps->stats[stRushMove] );
	PM_RushDebug( va( "%s swayed", m->name ) );
}

// Guard pressed (not held) in time: no damage, the attacker reels.
static void PM_RushParry( playerState_t *foe, bgRushMove_t *m ) {
	BG_RushStun( foe, RS_BLOCKSTUN, 120, ANIM_DEFLECT, 0, 0 );
	foe->stats[stRushFlags] &= ~RF_INT_ANY;
	foe->timers[tmRushGuard] = 0;
	BG_RushAutoLock( foe, pm->ps );
	PM_RushEvent( EV_RUSH_PARRY, pm->ps->stats[stRushMove] );
	BG_RushStun( pm->ps, RS_STUNNED, bg_rush.parryStagger, ANIM_STUNNED, 0, 0 );
	PM_RushDebug( va( "%s parried", m->name ) );
}

// The defender started its own strike with forward held just before this
// hit: its strike lands first.
static void PM_RushCounter( playerState_t *foe ) {
	int index, chain;
	index = foe->stats[stRushMove];
	chain = foe->stats[stRushChain];
	BG_RushRunResolved( foe, index, foe->stats[stRushFlags] & ( RF_SIDE_CHAIN | RF_COUNTER ) );
	foe->stats[stRushChain] = chain;
	BG_RushApplyHit( foe, pm->ps, index, RH_FORCE | RH_COUNTER );
}

static void BG_RushEnterClash( playerState_t *ps, int pose ) {
	BG_RushClearMove( ps );
	ps->stats[stRushState] = RS_CLASH;
	ps->timers[tmRushStun] = bg_rush.clashTime;
	VectorClear( ps->velocity );
	BG_RushPlay( ps, ANIM_BREAKER_MELEE_ATTACK1 + pose - 1 );
}

// Both strikes land together: the fists meet and both pick an answer.
static void PM_RushClash( playerState_t *foe ) {
	int pose;
	pose = ( ( pm->ps->commandTime / 50 ) % 6 ) + 1;
	BG_RushEnterClash( pm->ps, pose );
	BG_RushEnterClash( foe, ( pose % 6 ) + 1 );
	BG_RushAutoLock( foe, pm->ps );
	PM_RushEvent( EV_RUSH_CLASH, 0 );
	PM_RushDebug( "clash" );
}

// The hit lands now: decide hit, guard, defensive answer or whiff.
static void PM_RushResolve( playerState_t *foe ) {
	playerState_t	*ps;
	bgRushMove_t	*m, *fm;
	int				index, level, foeState, reach;
	float			dist;
	qboolean		back;

	ps = pm->ps;
	index = ps->stats[stRushMove];
	m = &bg_rush.moves[index];
	ps->stats[stRushFlags] |= RF_RESOLVED;
	if ( !foe ) {
		PM_RushWhiff( m, "no target" );
		return;
	}
	dist = Distance( ps->origin, foe->origin );
	reach = ( m->range > bg_rush.minSpacing ? m->range : bg_rush.minSpacing ) + bg_rush.reachSlack;
	if ( PM_RushTryVanish( foe, m, dist, reach ) ) {
		return;
	}
	if ( !PM_RushCanHit( foe ) ) {
		PM_RushWhiff( m, "target out of reach" );
		return;
	}
	if ( dist > reach ) {
		PM_RushWhiff( m, va( "distance %i > %i", (int)dist, reach ) );
		return;
	}

	level = BG_RushSmashLevel( ps );
	back = BG_RushBehind( foe, ps->origin );
	foeState = foe->stats[stRushState];

	if ( !back && foeState == RS_STARTUP ) {
		int myStart, foeStart;
		// Absolute start times, so the order in which the server runs the two
		// players inside a frame does not decide who pressed first.
		fm = &bg_rush.moves[foe->stats[stRushMove]];
		myStart = ps->commandTime - ps->timers[tmRushTime];
		foeStart = foe->commandTime - foe->timers[tmRushTime];
		// super counter: the defender pressed forward plus attack after this
		// strike began and close enough to the impact
		if ( ( foe->stats[stRushFlags] & RF_COUNTER ) && foeStart > myStart
			&& ps->commandTime - foeStart <= bg_rush.counterWindow ) {
			PM_RushCounter( foe );
			return;
		}
		// clash: its strike would land within the clash window of this one
		if ( foeStart + fm->startup - ps->commandTime <= bg_rush.clashWindow ) {
			PM_RushClash( foe );
			return;
		}
	}
	if ( !back && ( foeState == RS_NONE || foeState == RS_BLOCKSTUN ) && foe->timers[tmRushGuard] > 0 ) {
		if ( ( foe->stats[stRushFlags] & RF_INT_SWAY ) && foe->timers[tmRushGuard] <= bg_rush.swayWindow
			&& BG_RushPay( foe, bg_rush.swayCost ) ) {
			PM_RushSway( foe, m );
			return;
		}
		if ( ( foe->stats[stRushFlags] & RF_INT_PARRY ) && foe->timers[tmRushGuard] <= bg_rush.parryWindow
			&& !m->guardBreak && level < 3 ) {
			PM_RushParry( foe, m );
			return;
		}
	}
	BG_RushApplyHit( ps, foe, index, back ? RH_BACK : 0 );
}

/*
==============================================================================
Movement while a move runs
==============================================================================
*/

// Startup pulls the attacker to the distance at which the pose connects. A
// defender still flying (chase) is followed at its own speed.
static void PM_RushMagnet( playerState_t *foe, bgRushMove_t *m, int remaining ) {
	vec3_t	dir;
	float	dist, want, speed;
	int		range;
	VectorClear( pm->ps->velocity );
	if ( !foe || !PM_RushCanHit( foe ) ) {
		return;
	}
	if ( foe->timers[tmKnockback] > 0 ) {
		VectorCopy( foe->velocity, pm->ps->velocity );
	}
	range = m->range > bg_rush.minSpacing ? m->range : bg_rush.minSpacing;
	VectorSubtract( foe->origin, pm->ps->origin, dir );
	dist = VectorNormalize( dir );
	want = dist - range;
	if ( want > -2.0f && want < 2.0f ) {
		return;
	}
	if ( remaining < pml.msec ) {
		remaining = pml.msec;
	}
	speed = want * 1000.0f / remaining;
	if ( speed > bg_rush.magnetSpeed ) { speed = bg_rush.magnetSpeed; }
	if ( speed < -bg_rush.magnetSpeed ) { speed = -bg_rush.magnetSpeed; }
	VectorMA( pm->ps->velocity, speed, dir, pm->ps->velocity );
}

void PM_RushMove( void ) {
	PM_StepSlideMove( qfalse );
}

// Something else took over the player (a ki knockback, for example).
void PM_RushCancel( void ) {
	if ( pm->ps->stats[stRushState] == RS_NONE ) {
		return;
	}
	BG_RushClearMove( pm->ps );
	pm->ps->stats[stRushState] = RS_NONE;
}

static void PM_RushEnd( void ) {
	BG_RushClearMove( pm->ps );
	pm->ps->stats[stRushState] = RS_NONE;
	BG_RushNeutralAnim( pm->ps );
}

static void PM_RushMagnetRunning( playerState_t *foe ) {
	bgRushMove_t *m;
	if ( pm->ps->stats[stRushState] != RS_STARTUP ) {
		return;
	}
	m = &bg_rush.moves[pm->ps->stats[stRushMove]];
	PM_RushMagnet( foe, m, m->startup - pm->ps->timers[tmRushTime] );
}

/*
==============================================================================
Offense from neutral: rush-in, vanishing assault, chase
==============================================================================
*/

static qboolean PM_RushTryAssault( playerState_t *foe, float dist ) {
	playerState_t	*ps;
	vec3_t			angles, forward, offset;
	int				range;
	ps = pm->ps;
	if ( !( ps->stats[stRushFlags] & RF_INT_VANISH ) || ps->timers[tmRushGuard] <= 0
		|| ps->timers[tmRushGuard] > bg_rush.assaultWindow ) {
		return qfalse;
	}
	if ( !foe || BG_RushUntouchable( foe ) || dist > bg_rush.assaultRange ) {
		return qfalse;
	}
	if ( ps->powerLevel[plFatigue] < ps->powerLevel[plMaximum] * bg_rush.assaultCost ) {
		return qfalse;
	}
	range = bg_rush.moves[bg_rush.chain[0][0]].range;
	VectorSet( angles, 0, foe->viewangles[YAW], 0 );
	AngleVectors( angles, forward, NULL, NULL );
	VectorScale( forward, -range, offset );
	if ( !PM_RushTeleport( ps, foe->origin, offset, foe->clientNum ) ) {
		return qfalse;
	}
	BG_RushPay( ps, bg_rush.assaultCost );
	BG_RushFace( ps, foe->origin );
	ps->bitFlags &= ~usingZanzoken;
	ps->timers[tmZanzoken] = 0;
	ps->stats[stRushFlags] &= ~RF_INT_ANY;
	ps->timers[tmRushGuard] = 0;
	foe->timers[tmRushExposed] = bg_rush.assaultExposed;
	PM_RushEvent( EV_RUSH_VANISH, 0 );
	PM_RushStartFresh( foe );
	PM_RushDebug( "vanishing assault" );
	return qtrue;
}

static qboolean PM_RushTryRushIn( playerState_t *foe, float dist, qboolean alt ) {
	playerState_t *ps;
	ps = pm->ps;
	if ( !foe || BG_RushUntouchable( foe ) || dist > bg_rush.rushInRange ) {
		return qfalse;
	}
	if ( !( ps->bitFlags & usingBoost ) && !( pm->cmd.buttons & BUTTON_BOOST ) ) {
		return qfalse;
	}
	if ( !BG_RushPay( ps, bg_rush.rushInCost ) ) {
		return qfalse;
	}
	BG_RushClearMove( ps );
	ps->stats[stRushState] = RS_RUSHIN;
	ps->stats[stRushMove] = alt ? bg_rush.finisher[RDIR_BOOST] : bg_rush.chain[0][0];
	if ( alt ) {
		ps->stats[stRushFlags] |= RF_FINISHER;
	}
	BG_RushPlay( ps, ANIM_FLY_FORWARD );
	PM_RushEvent( EV_RUSH_SWING, 0 );
	PM_RushDebug( alt ? "rush-in to dropkick" : "rush-in" );
	return qtrue;
}

static qboolean PM_RushTryChase( playerState_t *foe, float dist ) {
	playerState_t	*ps;
	vec3_t			dir, offset;
	int				chases;
	ps = pm->ps;
	if ( !foe || foe->timers[tmKnockback] <= 0 || dist > bg_rush.chaseRange ) {
		return qfalse;
	}
	if ( foe->bitFlags & ( isDead | isUnconcious | isCrashed | isStruggling | isTransforming ) ) {
		return qfalse;
	}
	chases = RUSH_CHASES( ps );
	if ( chases >= bg_rush.chaseMax || chases > 2 ) {
		return qfalse;
	}
	if ( ps->powerLevel[plFatigue] < ps->powerLevel[plMaximum] * bg_rush.chaseCost[chases] ) {
		return qfalse;
	}
	// appear just ahead of its flight, facing it
	VectorCopy( foe->velocity, dir );
	if ( VectorNormalize( dir ) < 1.0f ) {
		VectorSubtract( ps->origin, foe->origin, dir );
		VectorNormalize( dir );
	}
	VectorScale( dir, bg_rush.chaseLead, offset );
	if ( !PM_RushTeleport( ps, foe->origin, offset, foe->clientNum ) ) {
		return qfalse;
	}
	BG_RushPay( ps, bg_rush.chaseCost[chases] );
	BG_RushFace( ps, foe->origin );
	ps->bitFlags &= ~usingZanzoken;
	ps->timers[tmZanzoken] = 0;
	BG_RushClearMove( ps );
	ps->stats[stRushState] = RS_CHASE;
	BG_RushSetCount( ps, chases + 1, RUSH_DUELS( ps ) );
	BG_RushNeutralAnim( ps );
	PM_RushEvent( EV_RUSH_VANISH, 0 );
	PM_RushDebug( va( "chase %i", chases + 1 ) );
	return qtrue;
}

/*
==============================================================================
Clash answer: attack beats alt, alt beats guard, guard beats attack
==============================================================================
*/

static int BG_RushClashChoice( playerState_t *ps ) {
	if ( ps->stats[stRushFlags] & RF_CLASH_RAPID ) { return 1; }
	if ( ps->stats[stRushFlags] & RF_CLASH_CHARGE ) { return 2; }
	if ( ps->stats[stRushFlags] & RF_CLASH_GUARD ) { return 3; }
	return 0;
}

static void PM_RushResolveClash( playerState_t *foe ) {
	playerState_t	*ps, *winner, *loser;
	bgRushMove_t	push;
	int				a, b, win, index;

	ps = pm->ps;
	a = BG_RushClashChoice( ps );
	b = BG_RushClashChoice( foe );
	if ( a == b ) { win = 0; }
	else if ( !a ) { win = -1; }
	else if ( !b ) { win = 1; }
	else if ( ( a == 1 && b == 2 ) || ( a == 2 && b == 3 ) || ( a == 3 && b == 1 ) ) { win = 1; }
	else { win = -1; }

	BG_RushInterrupt( ps );
	BG_RushInterrupt( foe );
	if ( !win ) {
		// nobody wins: both are thrown back a little
		push = bg_rush.moves[bg_rush.chain[0][0]];
		push.launch = 5;
		push.launchSpeed = bg_rush.chainEndSpeed * 0.8f;
		push.launchTime = 350;
		BG_RushLaunch( ps, foe, &push, 0 );
		BG_RushLaunch( foe, ps, &push, 0 );
		PM_RushEvent( EV_RUSH_CLASH, 0 );
		PM_RushDebug( va( "clash tie (%i / %i)", a, b ) );
		return;
	}
	winner = win > 0 ? ps : foe;
	loser = win > 0 ? foe : ps;
	index = bg_rush.finisherRepeat ? bg_rush.finisherRepeat : bg_rush.finisher[RDIR_NEUTRAL];
	BG_RushRunResolved( winner, index, RF_FINISHER );
	BG_RushApplyHit( winner, loser, index, RH_FORCE );
	PM_RushDebug( va( "clash won by cl%i (%i / %i)", winner->clientNum, a, b ) );
}

/*
==============================================================================
PM_Rush
==============================================================================
*/

void PM_Rush( void ) {
	playerState_t	*ps, *foe;
	bgRushMove_t	*m;
	int				state, time, total, startup;
	qboolean		atkPress, altPress, altRelease, charging, engaged;
	float			dist;

	ps = pm->ps;
	pm_rushEvents = 0;
	state = ps->stats[stRushState];
	atkPress = pml.rushAtkPress;
	altPress = pml.rushAltPress;
	altRelease = pml.rushAltRelease;

	// the old melee stats must stay quiet, other systems read them
	ps->stats[stMeleeState] = 0;
	ps->bitFlags &= ~usingMelee;

	if ( ps->persistant[PERS_TEAM] == TEAM_SPECTATOR || ( ps->bitFlags & ( isDead | isUnconcious | isStruggling ) ) ) {
		BG_RushInterrupt( ps );
		ps->stats[stRushCombo] = 0;
		return;
	}

	// keep the server pointers to the opponent fresh (g_active.c EV_MELEE_CHECK)
	ps->timers[tmUpdateMelee] += pml.msec;
	foe = PM_RushFoe();
	dist = foe ? Distance( ps->origin, foe->origin ) : 99999.0f;

	// ---- clash: both answer, the first to run out resolves for the pair ------
	if ( state == RS_CLASH ) {
		pml.rushEngaged = qtrue;
		PM_StopDirections();
		VectorClear( ps->velocity );
		if ( !( ps->stats[stRushFlags] & RF_CLASH_ANY ) ) {
			if ( atkPress ) { ps->stats[stRushFlags] |= RF_CLASH_RAPID; }
			else if ( altPress ) { ps->stats[stRushFlags] |= RF_CLASH_CHARGE; }
			else if ( pml.rushBlkPress ) { ps->stats[stRushFlags] |= RF_CLASH_GUARD; }
		}
		ps->timers[tmRushStun] -= pml.msec;
		if ( ps->timers[tmRushStun] <= 0 ) {
			if ( foe && foe->stats[stRushState] == RS_CLASH ) {
				PM_RushResolveClash( foe );
			}
			else {
				PM_RushEnd();
			}
		}
		return;
	}

	// ---- defender side ------------------------------------------------------
	if ( state >= RS_HITSTUN ) {
		pml.rushEngaged = qtrue;
		if ( pm->cmd.buttons & BUTTON_ATTACK ) { ps->pm_flags |= PMF_ATTACK1_HELD; }
		if ( pm->cmd.buttons & BUTTON_ALT_ATTACK ) { ps->pm_flags |= PMF_ATTACK2_HELD; }
		// Revenge: guard plus attack while reeling breaks the combo, paid
		// with fatigue
		if ( state == RS_HITSTUN && atkPress && ( pm->cmd.buttons & BUTTON_BLOCK ) && bg_rush.revenge
			&& foe && dist <= bg_rush.engageRange && !BG_RushUntouchable( foe ) && BG_RushPay( ps, bg_rush.revengeCost ) ) {
			ps->stats[stRushCombo] = 0;
			ps->timers[tmRushStun] = 0;
			BG_RushRunResolved( ps, bg_rush.revenge, RF_FINISHER );
			BG_RushApplyHit( ps, foe, bg_rush.revenge, RH_FORCE );
			PM_RushEvent( EV_RUSH_REVENGE, bg_rush.revenge );
			PM_RushDebug( "revenge counter" );
			return;
		}
		ps->timers[tmRushStun] -= pml.msec;
		PM_StopDirections();
		if ( ps->timers[tmRushStun] <= 0 ) {
			ps->timers[tmRushStun] = 0;
			ps->stats[stRushState] = RS_NONE;
			ps->stats[stRushCombo] = 0;
			BG_RushNeutralAnim( ps );
		}
		return;
	}
	if ( state == RS_NONE && ps->stats[stRushCombo] && !ps->timers[tmKnockback] ) {
		ps->stats[stRushCombo] = 0;		// back on its feet after a launch
	}

	// ---- attacker side ------------------------------------------------------
	charging = ( ps->weaponstate == WEAPON_CHARGING || ps->weaponstate == WEAPON_ALTCHARGING ) ? qtrue : qfalse;
	engaged = ( foe && !charging && dist <= bg_rush.engageRange && !BG_RushUntouchable( foe ) ) ? qtrue : qfalse;

	if ( ps->lockedTarget > 0 && ps->timers[tmUpdateMelee] >= 300 && state == RS_NONE ) {
		ps->timers[tmUpdateMelee] = 0;
		PM_RushEvent( EV_MELEE_CHECK, 0 );
	}

	// contextual buttons: in melee range the attack buttons belong to the
	// rush, and the weapon must not fire when the player walks out holding them
	pml.rushEngaged = ( engaged || state != RS_NONE ) ? qtrue : qfalse;
	if ( pml.rushEngaged ) {
		if ( pm->cmd.buttons & BUTTON_ATTACK ) { ps->pm_flags |= PMF_ATTACK1_HELD; }
		if ( pm->cmd.buttons & BUTTON_ALT_ATTACK ) { ps->pm_flags |= PMF_ATTACK2_HELD; }
	}

	// Chase: zanzoken on a launched defender, from neutral or from the
	// recovery of the move that launched it.
	if ( pml.rushTelePress && foe && !charging
		&& ( state == RS_NONE || ( state == RS_RECOVERY && ( ps->stats[stRushFlags] & RF_CONNECTED ) ) )
		&& PM_RushTryChase( foe, dist ) ) {
		state = RS_CHASE;
		pml.rushEngaged = qtrue;
	}

	if ( state == RS_NONE ) {
		if ( !charging && atkPress && foe && PM_RushTryAssault( foe, dist ) ) {
			PM_RushMagnetRunning( foe );
			return;
		}
		if ( !engaged ) {
			// boost plus attack at mid range dashes in instead of firing
			if ( !charging && ( atkPress || altPress ) && PM_RushTryRushIn( foe, dist, altPress && !atkPress ) ) {
				pml.rushEngaged = qtrue;
				ps->pm_flags |= PMF_ATTACK1_HELD | PMF_ATTACK2_HELD;
				state = RS_RUSHIN;
			}
			else {
				return;
			}
		}
		else if ( atkPress ) {
			PM_RushStartFresh( foe );
			PM_RushMagnetRunning( foe );
			return;
		}
		else if ( altPress ) {
			BG_RushClearMove( ps );
			ps->stats[stRushState] = RS_CHARGE;
			PM_RushEvent( EV_RUSH_CHARGE, 0 );
			PM_RushDebug( "smash charge" );
			state = RS_CHARGE;
			pml.rushEngaged = qtrue;
		}
		else {
			return;
		}
	}

	// ---- rush-in: dash at the defender, the move starts on arrival ----------
	if ( state == RS_RUSHIN ) {
		int		index, range;
		vec3_t	dir;
		float	speed;
		ps->timers[tmRushTime] += pml.msec;
		index = ps->stats[stRushMove];
		if ( !foe || BG_RushUntouchable( foe ) || ps->timers[tmRushTime] > bg_rush.rushInMax || index <= 0 ) {
			PM_RushEnd();
			return;
		}
		range = bg_rush.moves[index].range > bg_rush.minSpacing ? bg_rush.moves[index].range : bg_rush.minSpacing;
		if ( dist <= range + bg_rush.reachSlack ) {
			if ( ps->stats[stRushFlags] & RF_FINISHER ) {
				PM_RushStartMove( index, 0, RF_FINISHER );
			}
			else {
				PM_RushStartFresh( foe );
			}
			PM_RushMagnetRunning( foe );
			return;
		}
		VectorSubtract( foe->origin, ps->origin, dir );
		VectorNormalize( dir );
		speed = ( dist - range ) * 1000.0f / pml.msec;
		if ( speed > bg_rush.rushInSpeed ) {
			speed = bg_rush.rushInSpeed;
		}
		VectorScale( dir, speed, ps->velocity );
		ps->eFlags |= EF_AURA;
		return;
	}

	// ---- chase: riding next to the launched defender, pick the strike ------
	if ( state == RS_CHASE ) {
		ps->timers[tmRushTime] += pml.msec;
		if ( !foe || foe->timers[tmKnockback] <= 0 || ( foe->bitFlags & ( isCrashed | isDead ) )
			|| ps->timers[tmRushTime] > bg_rush.chaseWindow ) {
			PM_RushEnd();
			return;
		}
		VectorCopy( foe->velocity, ps->velocity );
		if ( atkPress || altPress || ( pm->cmd.buttons & ( BUTTON_ATTACK | BUTTON_ALT_ATTACK ) ) ) {
			int dir, index;
			dir = PM_RushDir();
			index = dir == RDIR_NEUTRAL ? bg_rush.finisherRepeat : bg_rush.finisher[dir];
			if ( !index ) {
				index = bg_rush.finisher[RDIR_NEUTRAL];
			}
			PM_RushStartMove( index, 0, RF_FINISHER | RF_CHASED );
			PM_RushMagnetRunning( foe );
		}
		return;
	}

	// ---- smash charge ---------------------------------------------------------
	if ( state == RS_CHARGE ) {
		int index, anim;
		VectorClear( ps->velocity );
		ps->timers[tmRushTime] += pml.msec;
		index = bg_rush.smash[PM_RushDir()];
		anim = index ? bg_rush.moves[index].anim : ANIM_POWER_MELEE_1_HIT;
		if ( anim >= ANIM_POWER_MELEE_1_HIT && anim <= ANIM_POWER_MELEE_6_HIT ) {
			anim -= ANIM_POWER_MELEE_1_HIT - ANIM_POWER_MELEE_1_CHARGE;
		}
		if ( ( ps->legsAnim & ~ANIM_TOGGLEBIT ) != anim ) {
			BG_RushPlay( ps, anim );
		}
		if ( altRelease || !( pm->cmd.buttons & BUTTON_ALT_ATTACK ) || ps->timers[tmRushTime] >= bg_rush.smashMax ) {
			PM_RushStartSmash();
			PM_RushMagnetRunning( foe );
		}
		return;
	}

	// ---- a move is running: STARTUP -> ACTIVE -> RECOVERY -------------------
	if ( !ps->stats[stRushMove] ) {
		PM_RushEnd();
		return;
	}
	m = &bg_rush.moves[ps->stats[stRushMove]];
	if ( state == RS_STARTUP || state == RS_ACTIVE ) {
		if ( atkPress ) { ps->stats[stRushFlags] |= RF_BUF_ATK; }
		if ( altPress ) { ps->stats[stRushFlags] |= RF_BUF_ALT; }
	}
	startup = m->startup;
	total = m->startup + m->active + m->recovery;
	time = ps->timers[tmRushTime] + pml.msec;
	ps->timers[tmRushTime] = time;

	if ( state == RS_STARTUP ) {
		if ( time < startup ) {
			PM_RushMagnet( foe, m, startup - time );
			return;
		}
		VectorClear( ps->velocity );
		ps->stats[stRushState] = state = RS_ACTIVE;
		PM_RushResolve( foe );
		if ( ps->stats[stRushState] != RS_ACTIVE ) {
			return;		// parried, countered or clashed
		}
		time = ps->timers[tmRushTime];		// a sway or vanish extended the recovery
	}
	VectorClear( ps->velocity );
	if ( state == RS_ACTIVE && time >= startup + m->active ) {
		ps->stats[stRushState] = state = RS_RECOVERY;
	}
	if ( state != RS_RECOVERY ) {
		return;
	}

	// RECOVERY: the chain continues only from a move that connected
	if ( ps->stats[stRushFlags] & RF_CONNECTED ) {
		qboolean wantAlt, wantAtk;
		int which;
		wantAlt = ( altPress || ( ps->stats[stRushFlags] & RF_BUF_ALT ) ) ? qtrue : qfalse;
		wantAtk = ( atkPress || ( ps->stats[stRushFlags] & RF_BUF_ATK ) ) ? qtrue : qfalse;
		which = ( ps->stats[stRushFlags] & RF_SIDE_CHAIN ) ? 1 : 0;
		if ( !( ps->stats[stRushFlags] & RF_FINISHER ) && engaged ) {
			if ( wantAlt ) {
				PM_RushStartFinisher();
				PM_RushMagnetRunning( foe );
				return;
			}
			if ( wantAtk && ps->stats[stRushChain] < bg_rush.chainLen[which] ) {
				PM_RushStartChain( ps->stats[stRushChain] + 1, 0 );
				PM_RushMagnetRunning( foe );
				return;
			}
		}
	}
	// not consumed (whiff, finisher, end of chain): run it when the move ends
	if ( atkPress ) {
		ps->stats[stRushFlags] |= RF_BUF_ATK;
	}

	if ( time >= total ) {
		qboolean again;
		again = ( ( ps->stats[stRushFlags] & RF_BUF_ATK ) && engaged ) ? qtrue : qfalse;
		PM_RushEnd();
		PM_RushDebug( "back to neutral" );
		if ( again ) {
			PM_RushStartFresh( foe );
			PM_RushMagnetRunning( foe );
		}
	}
}
