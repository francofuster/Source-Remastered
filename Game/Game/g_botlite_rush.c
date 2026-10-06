#include "g_local.h"
#include "g_botlite.h"

/*
 * Combate Rush (g_rushCombat 1): tactica de BotLite para el melee nuevo.
 *
 * Lee el estado rush del rival directamente de su playerState (el bot vive en
 * el servidor) y responde segun el nivel (1..3) y la personalidad del bot.
 *
 * Dos entradas:
 *   - BotLite_RunRushReactions: corre en CUALQUIER modo del bot, antes de
 *     elegir goal. Revancha mientras recibe un combo, respuesta a un choque,
 *     y defensa ante un golpe que llega (guardia, parada, esquiva emparejada,
 *     alejarse con boost, zanzoken o contraataque) o ante un smash cargando.
 *   - BotLite_RunRushCombat: en el goal de combate. Ataque: cadena con remate
 *     en un paso elegido al empezarla, smash contra guardias, ataque
 *     desvanecido, embestida a media distancia y persecucion del lanzado.
 *
 * La tabla por nivel se carga de botsys/skills/*.cfg (claves rush_*); los
 * valores de abajo son los de respaldo.
 *
 * NOTA: sin apostrofes en los comentarios (q3cpp de la toolchain QVM).
 */

enum {
	BOTLITE_RUSH_ANSWER_NONE,
	BOTLITE_RUSH_ANSWER_GUARD,
	BOTLITE_RUSH_ANSWER_PARRY,
	BOTLITE_RUSH_ANSWER_SWAY,
	BOTLITE_RUSH_ANSWER_MOVE,
	BOTLITE_RUSH_ANSWER_VANISH,
	BOTLITE_RUSH_ANSWER_COUNTER
};

static botlite_rush_skill_t botlite_rushSkill[4];	/* 1..3 */
static qboolean botlite_rushSkillReady;

void BotLite_RushResetSkillDefaults( void ) {
	botlite_rush_skill_t *s;
	memset( botlite_rushSkill, 0, sizeof( botlite_rushSkill ) );
	botlite_rushSkillReady = qtrue;

	s = &botlite_rushSkill[1];
	s->guard = 0.25f; s->parry = 0.00f; s->sway = 0.00f; s->moveDodge = 0.05f; s->vanish = 0.00f; s->counter = 0.00f;
	s->revenge = 0.00f; s->chargeDodge = 0.10f; s->chase = 0.00f; s->finisher = 0.40f; s->smashOnGuard = 0.20f;
	s->assault = 0.00f; s->rushIn = 0.15f; s->crashFinisherChance = 0.00f;
	s->clashDelay = 350; s->offenseGapMin = 450; s->offenseGapMax = 900;

	s = &botlite_rushSkill[2];
	s->guard = 0.40f; s->parry = 0.08f; s->sway = 0.05f; s->moveDodge = 0.06f; s->vanish = 0.03f; s->counter = 0.02f;
	s->revenge = 0.06f; s->chargeDodge = 0.35f; s->chase = 0.40f; s->finisher = 0.65f; s->smashOnGuard = 0.50f;
	s->assault = 0.08f; s->rushIn = 0.35f; s->crashFinisherChance = 0.35f;
	s->clashDelay = 220; s->offenseGapMin = 250; s->offenseGapMax = 600;

	s = &botlite_rushSkill[3];
	s->guard = 0.45f; s->parry = 0.18f; s->sway = 0.10f; s->moveDodge = 0.08f; s->vanish = 0.10f; s->counter = 0.06f;
	s->revenge = 0.15f; s->chargeDodge = 0.60f; s->chase = 0.80f; s->finisher = 0.85f; s->smashOnGuard = 0.80f;
	s->assault = 0.20f; s->rushIn = 0.60f; s->crashFinisherChance = 0.60f;
	s->clashDelay = 120; s->offenseGapMin = 120; s->offenseGapMax = 350;
}

const botlite_rush_skill_t *BotLite_RushSkill( int skill ) {
	if ( !botlite_rushSkillReady ) {
		BotLite_BotsysInit();		/* loads the defaults and the botsys cfg files */
		if ( !botlite_rushSkillReady ) {
			BotLite_RushResetSkillDefaults();
		}
	}
	if ( skill < 1 ) { skill = 1; }
	if ( skill > 3 ) { skill = 3; }
	return &botlite_rushSkill[skill];
}

/* Claves rush_* de botsys/skills/*.cfg. Devuelve qtrue si la clave era suya. */
qboolean BotLite_RushApplySkillKey( int skill, const char *key, const char *value ) {
	botlite_rush_skill_t	*s;
	float					pct;
	int						i;

	if ( skill < 1 || skill > 3 || !key || Q_stricmpn( key, "rush_", 5 ) ) {
		return qfalse;
	}
	s = &botlite_rushSkill[skill];
	i = atoi( value );
	pct = atof( value ) / 100.0f;
	if ( pct < 0.0f ) { pct = 0.0f; }
	if ( pct > 1.0f ) { pct = 1.0f; }

	if ( !Q_stricmp( key, "rush_guard_pct" ) ) { s->guard = pct; }
	else if ( !Q_stricmp( key, "rush_parry_pct" ) ) { s->parry = pct; }
	else if ( !Q_stricmp( key, "rush_sway_pct" ) ) { s->sway = pct; }
	else if ( !Q_stricmp( key, "rush_move_dodge_pct" ) ) { s->moveDodge = pct; }
	else if ( !Q_stricmp( key, "rush_vanish_pct" ) ) { s->vanish = pct; }
	else if ( !Q_stricmp( key, "rush_counter_pct" ) ) { s->counter = pct; }
	else if ( !Q_stricmp( key, "rush_revenge_pct" ) ) { s->revenge = pct; }
	else if ( !Q_stricmp( key, "rush_charge_dodge_pct" ) ) { s->chargeDodge = pct; }
	else if ( !Q_stricmp( key, "rush_chase_pct" ) ) { s->chase = pct; }
	else if ( !Q_stricmp( key, "rush_finisher_pct" ) ) { s->finisher = pct; }
	else if ( !Q_stricmp( key, "rush_smash_on_guard_pct" ) ) { s->smashOnGuard = pct; }
	else if ( !Q_stricmp( key, "rush_assault_pct" ) ) { s->assault = pct; }
	else if ( !Q_stricmp( key, "rush_rushin_pct" ) ) { s->rushIn = pct; }
	else if ( !Q_stricmp( key, "rush_crash_finisher_pct" ) ) { s->crashFinisherChance = pct; }
	else if ( !Q_stricmp( key, "rush_clash_reaction_ms" ) ) { s->clashDelay = i; }
	else if ( !Q_stricmp( key, "rush_offense_gap_min_ms" ) ) { s->offenseGapMin = i; }
	else if ( !Q_stricmp( key, "rush_offense_gap_max_ms" ) ) { s->offenseGapMax = i; }
	else {
		G_Printf( "BotLite: unknown rush key %s\n", key );
	}
	if ( s->offenseGapMax < s->offenseGapMin ) {
		s->offenseGapMax = s->offenseGapMin;
	}
	return qtrue;
}

static int BotLite_RushRandomDir( float specialTendency ) {
	float r;
	r = random();
	if ( r < 0.30f ) { return RDIR_NEUTRAL; }
	if ( r < 0.50f ) { return RDIR_UP; }
	if ( r < 0.65f ) { return RDIR_DOWN; }
	if ( r < 0.78f ) { return RDIR_LEFT; }
	if ( r < 0.91f ) { return RDIR_RIGHT; }
	return specialTendency > 0.5f ? RDIR_BOOST : RDIR_NEUTRAL;
}

/* Las direcciones del remate y del smash se leen del usercmd. */
static void BotLite_RushHoldDir( gentity_t *bot, int dir ) {
	switch ( dir ) {
	case RDIR_UP:		BotLite_EA_MoveUp( bot, 127 ); break;
	case RDIR_DOWN:		BotLite_EA_MoveUp( bot, -127 ); break;
	case RDIR_LEFT:		BotLite_EA_MoveRight( bot, -127 ); break;
	case RDIR_RIGHT:	BotLite_EA_MoveRight( bot, 127 ); break;
	case RDIR_BOOST:
		BotLite_EA_Button( bot, BUTTON_BOOST );
		BotLite_EA_AllowBoostWithAttack( bot );
		break;
	default:
		break;
	}
}

/* alejarse en diagonal hacia atras con boost */
static void BotLite_RushBackOff( gentity_t *bot, int side ) {
	BotLite_EA_MoveForward( bot, -127 );
	BotLite_EA_MoveRight( bot, side );
	BotLite_EA_Button( bot, BUTTON_BOOST );
}

static int BotLite_RushMoveStart( playerState_t *ps ) {
	return ps->commandTime - ps->timers[tmRushTime];
}

static int BotLite_RushRemaining( playerState_t *ps ) {
	int move;
	move = ps->stats[stRushMove];
	if ( move <= 0 || move > bg_rush.numMoves ) {
		return 9999;
	}
	return bg_rush.moves[move].startup - ps->timers[tmRushTime];
}

static float BotLite_RushFatigue( playerState_t *ps ) {
	return ps->powerLevel[plMaximum] > 0 ? (float)ps->powerLevel[plFatigue] / ps->powerLevel[plMaximum] : 0.0f;
}

/* Quien puede pegarle al bot: con lock-on por parejas, su pareja de lock-on. */
static gentity_t *BotLite_RushAttacker( gentity_t *bot, const botlite_snapshot_t *snapshot ) {
	gentity_t *other;
	if ( bot->client->ps.lockedTarget > 0 && bot->client->ps.lockedTarget <= level.maxclients ) {
		other = &g_entities[bot->client->ps.lockedTarget - 1];
		if ( other->inuse && other->client && other != bot ) {
			return other;
		}
	}
	if ( snapshot && snapshot->target && snapshot->target->client ) {
		return snapshot->target;
	}
	return NULL;
}

/* ---- defensa ante un golpe entrante -------------------------------------- */

static void BotLite_RushPickAnswer( botlite_info_t *info, const botlite_rush_skill_t *sk, playerState_t *ps ) {
	float r, guard, fatigue;
	fatigue = BotLite_RushFatigue( ps );
	guard = sk->guard * ( 0.6f + info->blockTendency * 0.8f );
	r = random();
	info->rush.threatResponse = BOTLITE_RUSH_ANSWER_NONE;
	if ( ( r -= sk->counter ) < 0 ) { info->rush.threatResponse = BOTLITE_RUSH_ANSWER_COUNTER; return; }
	if ( ( r -= sk->parry ) < 0 ) { info->rush.threatResponse = BOTLITE_RUSH_ANSWER_PARRY; return; }
	if ( ( r -= sk->sway ) < 0 && fatigue > 0.2f ) { info->rush.threatResponse = BOTLITE_RUSH_ANSWER_SWAY; return; }
	if ( ( r -= sk->moveDodge ) < 0 && fatigue > 0.15f ) { info->rush.threatResponse = BOTLITE_RUSH_ANSWER_MOVE; return; }
	if ( ( r -= sk->vanish ) < 0 && fatigue > 0.3f ) { info->rush.threatResponse = BOTLITE_RUSH_ANSWER_VANISH; return; }
	if ( ( r -= guard ) < 0 ) { info->rush.threatResponse = BOTLITE_RUSH_ANSWER_GUARD; return; }
}

/* qtrue si el bot esta respondiendo a un golpe en camino */
static qboolean BotLite_RushDefend( gentity_t *bot, botlite_info_t *info, const botlite_rush_skill_t *sk,
								   playerState_t *ps, playerState_t *tps, float dist ) {
	int key, remaining, myState;
	myState = ps->stats[stRushState];
	if ( tps->stats[stRushState] != RS_STARTUP || dist > 160.0f ) {
		/* el golpe ya paso: mantener la guardia mientras siga activo */
		if ( info->rush.threatResponse == BOTLITE_RUSH_ANSWER_GUARD && tps->stats[stRushState] == RS_ACTIVE ) {
			BotLite_EA_Button( bot, BUTTON_BLOCK );
			return qtrue;
		}
		return qfalse;
	}
	key = BotLite_RushMoveStart( tps ) * 31 + tps->stats[stRushMove];
	if ( key != info->rush.threatKey ) {
		info->rush.threatKey = key;
		BotLite_RushPickAnswer( info, sk, ps );
		info->rush.chargeSide = ( random() < 0.5f ) ? 127 : -127;
	}
	if ( myState != RS_NONE && myState != RS_BLOCKSTUN
		&& !( myState == RS_RECOVERY && info->rush.threatResponse == BOTLITE_RUSH_ANSWER_VANISH ) ) {
		return qfalse;
	}
	remaining = BotLite_RushRemaining( tps );
	switch ( info->rush.threatResponse ) {
	case BOTLITE_RUSH_ANSWER_GUARD:
		BotLite_EA_Button( bot, BUTTON_BLOCK );
		return qtrue;
	case BOTLITE_RUSH_ANSWER_PARRY:
		if ( remaining <= 90 && !( ps->stats[stRushFlags] & RF_BLK_HELD ) ) {
			BotLite_EA_Button( bot, BUTTON_BLOCK );
		}
		return qtrue;
	case BOTLITE_RUSH_ANSWER_SWAY:
		if ( remaining <= 100 && !( ps->stats[stRushFlags] & RF_BLK_HELD ) ) {
			BotLite_EA_Button( bot, BUTTON_BLOCK );
			BotLite_EA_MoveRight( bot, info->rush.chargeSide );
		}
		return qtrue;
	case BOTLITE_RUSH_ANSWER_MOVE:
		if ( myState == RS_NONE ) {
			BotLite_RushBackOff( bot, info->rush.chargeSide );
		}
		return qtrue;
	case BOTLITE_RUSH_ANSWER_VANISH:
		if ( remaining <= 100 && !( ps->stats[stRushFlags] & RF_TELE_HELD ) ) {
			BotLite_EA_Button( bot, BUTTON_TELEPORT );
		}
		return qtrue;
	case BOTLITE_RUSH_ANSWER_COUNTER:
		if ( remaining <= 80 && myState == RS_NONE ) {
			BotLite_EA_MoveForward( bot, 127 );
			BotLite_EA_Button( bot, BUTTON_ATTACK );
		}
		return qtrue;
	default:
		return qfalse;
	}
}

/* Un smash cargando se ve venir: alejarse, o aguantar con guardia (el nivel 3
 * la rompe, por eso los niveles altos prefieren moverse). */
static qboolean BotLite_RushAnswerCharge( gentity_t *bot, botlite_info_t *info, const botlite_rush_skill_t *sk,
										 playerState_t *ps, playerState_t *tps, float dist ) {
	int key;
	if ( tps->stats[stRushState] != RS_CHARGE || dist > 220.0f || ps->stats[stRushState] != RS_NONE ) {
		return qfalse;
	}
	key = BotLite_RushMoveStart( tps ) + 1;
	if ( key != info->rush.chargeKey ) {
		info->rush.chargeKey = key;
		info->rush.chargeSide = ( random() < 0.5f ) ? 127 : -127;
		if ( random() < sk->chargeDodge && BotLite_RushFatigue( ps ) > 0.15f ) {
			info->rush.chargeAnswer = 1;
		}
		else if ( random() < sk->guard ) {
			info->rush.chargeAnswer = 2;
		}
		else {
			info->rush.chargeAnswer = 0;
		}
	}
	if ( info->rush.chargeAnswer == 1 ) {
		BotLite_RushBackOff( bot, info->rush.chargeSide );
		return qtrue;
	}
	if ( info->rush.chargeAnswer == 2 ) {
		BotLite_EA_Button( bot, BUTTON_BLOCK );
		return qtrue;
	}
	return qfalse;
}

/*
 * Reacciones que valen en cualquier modo del bot (combate, espera tras un
 * choque, recuperacion, subida tras estrellarse...). Corre antes de elegir
 * goal porque un golpe cuerpo a cuerpo llega en 60-160 ms.
 */
qboolean BotLite_RunRushReactions( gentity_t *bot, int clientNum, const botlite_snapshot_t *snapshot ) {
	botlite_info_t				*info;
	const botlite_rush_skill_t	*sk;
	gentity_t					*attacker;
	playerState_t				*ps, *tps;
	int							myState;
	float						dist;

	if ( !g_rushCombat.integer || !bot || !bot->client ) {
		return qfalse;
	}
	info = &g_botlite[clientNum];
	sk = BotLite_RushSkill( info->skill );
	ps = &bot->client->ps;
	myState = ps->stats[stRushState];
	if ( ps->stats[stRushCombo] == 0 ) {
		info->rush.revengeRolled = qfalse;
	}
	attacker = BotLite_RushAttacker( bot, snapshot );

	/* reeling: solo cabe la revancha */
	if ( myState >= RS_HITSTUN ) {
		if ( attacker && myState == RS_HITSTUN && ps->stats[stRushCombo] >= 2 && !info->rush.revengeRolled ) {
			info->rush.revengeRolled = qtrue;
			if ( random() < sk->revenge && BotLite_RushFatigue( ps ) > 0.3f ) {
				BotLite_EA_AllowBlockWithAttack( bot );
				BotLite_EA_Button( bot, BUTTON_BLOCK | BUTTON_ATTACK );
			}
		}
		return qtrue;
	}

	/* choque: responder tras el tiempo de reaccion */
	if ( myState == RS_CLASH ) {
		if ( !info->rush.clashAnswerTime ) {
			float r;
			info->rush.clashAnswerTime = level.time + sk->clashDelay + (int)( random() * 120.0f );
			r = random();
			info->rush.clashAnswer = r < 0.45f ? BUTTON_ATTACK : ( r < 0.75f ? BUTTON_ALT_ATTACK : BUTTON_BLOCK );
		}
		if ( level.time >= info->rush.clashAnswerTime ) {
			BotLite_EA_Button( bot, info->rush.clashAnswer );
		}
		return qtrue;
	}
	info->rush.clashAnswerTime = 0;

	if ( !attacker ) {
		return qfalse;
	}
	tps = &attacker->client->ps;
	dist = Distance( ps->origin, tps->origin );
	if ( BotLite_RushDefend( bot, info, sk, ps, tps, dist ) ) {
		return qtrue;
	}
	return BotLite_RushAnswerCharge( bot, info, sk, ps, tps, dist );
}

/* ---- ataque ---------------------------------------------------------------- */

static void BotLite_RushStartPlan( botlite_info_t *info, const botlite_rush_skill_t *sk ) {
	float chance;
	chance = sk->finisher * ( 0.7f + info->comboCommitment * 0.6f );
	if ( random() < chance ) {
		info->rush.finisherStep = 1 + (int)( random() * 4.0f );	/* 1..4 */
		info->rush.finisherDir = BotLite_RushRandomDir( info->specialTendency );
	}
	else {
		info->rush.finisherStep = 0;
	}
}

static void BotLite_RushOffense( gentity_t *bot, botlite_info_t *info, const botlite_rush_skill_t *sk,
								playerState_t *ps, playerState_t *tps ) {
	int myState, tState, gap;
	qboolean targetGuarding, targetReeling;
	myState = ps->stats[stRushState];
	tState = tps->stats[stRushState];
	targetGuarding = ( ( tps->bitFlags & usingBlock ) && ( tState == RS_NONE || tState == RS_BLOCKSTUN ) ) ? qtrue : qfalse;
	targetReeling = ( tState >= RS_HITSTUN && tState != RS_DODGE ) ? qtrue : qfalse;

	/* smash cargado: mantener hasta el momento elegido y soltar con direccion */
	if ( myState == RS_CHARGE ) {
		if ( level.time < info->rush.smashUntil ) {
			BotLite_EA_Button( bot, BUTTON_ALT_ATTACK );
		}
		else {
			BotLite_RushHoldDir( bot, info->rush.smashDir );
		}
		return;
	}

	/* cadena en curso: seguir o rematar en el paso elegido */
	if ( myState == RS_STARTUP || myState == RS_ACTIVE || myState == RS_RECOVERY ) {
		if ( ps->stats[stRushFlags] & RF_FINISHER ) {
			return;
		}
		if ( info->rush.finisherStep && ps->stats[stRushChain] >= info->rush.finisherStep ) {
			BotLite_RushHoldDir( bot, info->rush.finisherDir );
			BotLite_EA_Button( bot, BUTTON_ALT_ATTACK );
			return;
		}
		BotLite_EA_Button( bot, BUTTON_ATTACK );
		return;
	}
	if ( myState != RS_NONE ) {
		return;
	}

	/* el rival aturdido o en recuperacion: castigar ya */
	if ( targetReeling || tState == RS_RECOVERY ) {
		BotLite_RushStartPlan( info, sk );
		BotLite_EA_Button( bot, BUTTON_ATTACK );
		return;
	}
	if ( level.time < info->rush.nextOffenseTime ) {
		return;
	}
	gap = sk->offenseGapMax - sk->offenseGapMin;
	info->rush.nextOffenseTime = level.time + sk->offenseGapMin + (int)( random() * gap * ( 1.3f - info->aggression * 0.6f ) );

	if ( targetGuarding ) {
		if ( random() < sk->assault && BotLite_RushFatigue( ps ) > 0.4f ) {
			info->rush.assaultStage = 1;
			info->rush.assaultTime = level.time + 100;
			BotLite_EA_Button( bot, BUTTON_TELEPORT );
			return;
		}
		if ( random() < sk->smashOnGuard ) {
			/* nivel 3 para romper la guardia */
			info->rush.smashUntil = level.time + bg_rush.smashLevel3 + 50;
			info->rush.smashDir = BotLite_RushRandomDir( info->specialTendency );
			BotLite_EA_Button( bot, BUTTON_ALT_ATTACK );
			return;
		}
	}
	else if ( random() < 0.15f + info->specialTendency * 0.2f ) {
		info->rush.smashUntil = level.time + 200 + (int)( random() * 800.0f );
		info->rush.smashDir = BotLite_RushRandomDir( info->specialTendency );
		BotLite_EA_Button( bot, BUTTON_ALT_ATTACK );
		return;
	}
	BotLite_RushStartPlan( info, sk );
	BotLite_EA_Button( bot, BUTTON_ATTACK );
}

/* ---- goal de combate ------------------------------------------------------- */

qboolean BotLite_RunRushCombat( gentity_t *bot, int clientNum, const botlite_snapshot_t *snapshot ) {
	botlite_info_t				*info;
	const botlite_rush_skill_t	*sk;
	gentity_t					*target;
	playerState_t				*ps, *tps;
	int							myState;
	float						dist;

	if ( !g_rushCombat.integer || !bot || !bot->client || !snapshot || !snapshot->target || !snapshot->target->client ) {
		return qfalse;
	}
	if ( BotLite_RunRushReactions( bot, clientNum, snapshot ) ) {
		return qtrue;
	}
	info = &g_botlite[clientNum];
	sk = BotLite_RushSkill( info->skill );
	target = snapshot->target;
	ps = &bot->client->ps;
	tps = &target->client->ps;
	myState = ps->stats[stRushState];
	dist = Distance( ps->origin, tps->origin );

	/* el lock-on es la puerta del melee (y por parejas) */
	if ( ps->lockedTarget != target->s.number + 1 ) {
		BotLite_SetLockOn( bot, target );
		if ( ps->lockedTarget != target->s.number + 1 ) {
			return qfalse;
		}
	}

	/* persecucion del rival lanzado: una tirada por vuelo, y otra tras cada
	 * golpe de persecucion (el rival sale volando de nuevo) */
	if ( myState == RS_CHASE ) {
		info->rush.chaseKey = 0;
		BotLite_RushHoldDir( bot, info->rush.chaseDir );
		BotLite_EA_Button( bot, BUTTON_ATTACK );
		return qtrue;
	}
	if ( tps->timers[tmKnockback] <= 0 ) {
		info->rush.chaseKey = 0;
	}
	else if ( myState == RS_NONE || myState == RS_RECOVERY ) {
		if ( !info->rush.chaseKey ) {
			info->rush.chaseKey = 1;
			info->rush.chaseWanted = ( random() < sk->chase * ( 0.6f + info->aggression * 0.6f ) ) ? qtrue : qfalse;
			info->rush.chaseDir = BotLite_RushRandomDir( info->specialTendency );
		}
		if ( info->rush.chaseWanted && ( ps->stats[stRushCount] & 15 ) < bg_rush.chaseMax
			&& dist <= bg_rush.chaseRange && !( ps->stats[stRushFlags] & RF_TELE_HELD ) ) {
			BotLite_EA_Button( bot, BUTTON_TELEPORT );
			return qtrue;
		}
		return qfalse;
	}

	/* ataque desvanecido: tras el zanzoken, el ataque */
	if ( info->rush.assaultStage == 1 ) {
		if ( level.time >= info->rush.assaultTime ) {
			info->rush.assaultStage = 0;
			BotLite_RushStartPlan( info, sk );
			BotLite_EA_Button( bot, BUTTON_ATTACK );
		}
		return qtrue;
	}

	/* embestida en curso */
	if ( myState == RS_RUSHIN ) {
		return qtrue;
	}

	if ( dist <= bg_rush.engageRange ) {
		/* a distancia de golpe: el iman de cada golpe hace el resto */
		if ( dist > bg_rush.engageRange - 25.0f && myState == RS_NONE ) {
			BotLite_EA_MoveForward( bot, 127 );
		}
		BotLite_RushOffense( bot, info, sk, ps, tps );
		return qtrue;
	}
	if ( myState != RS_NONE ) {
		BotLite_RushOffense( bot, info, sk, ps, tps );
		return qtrue;
	}

	/* media distancia: embestida con boost de vez en cuando */
	if ( dist <= bg_rush.rushInRange - 50 && dist > bg_rush.engageRange + 20 && level.time >= info->rush.nextRushInTime
		&& !( tps->timers[tmKnockback] > 0 ) && BotLite_RushFatigue( ps ) > 0.25f ) {
		info->rush.nextRushInTime = level.time + 1000;
		if ( random() < sk->rushIn * ( 0.5f + info->rushTendency ) ) {
			BotLite_RushStartPlan( info, sk );
			BotLite_EA_Button( bot, BUTTON_BOOST | BUTTON_ATTACK );
			BotLite_EA_AllowBoostWithAttack( bot );
			return qtrue;
		}
	}
	return qfalse;
}
