#include "g_local.h"
#include "g_botlite.h"

/*
 * ============================================================================
 * Forcejeo de haces (power struggle)
 * ============================================================================
 *
 * Mecanica real, verificada en el codigo:
 *
 *  - g_usermissile.c:443 -- Think_NormalMissileStruggle solo suma potencia al haz
 *    propio si el duenio tiene usingBoost activo Y powerLevel[plCurrent] > 1.
 *    Es la UNICA entrada del jugador que altera el resultado del forcejeo.
 *
 *  - bg_pmove.c:811 -- PM_CheckBoost hace early-return mientras isStruggling este
 *    activo. El estado de boost queda por lo tanto CONGELADO en el instante del
 *    choque: no se puede empezar ni dejar de boostear una vez empezado el forcejeo.
 *
 * Consecuencia de diseno: la decision se toma ANTES del choque. Un bot que no venia
 * boosteando mientras cargaba o disparaba el haz pierde el forcejeo haga lo que haga
 * despues. Por eso este modulo actua en dos momentos distintos:
 *
 *   BotLite_StrugglePrepare()  -> mientras carga/dispara un haz: engancha el boost
 *                                 para que el flag este puesto si hay choque.
 *   BotLite_RunStruggle()      -> ya dentro del forcejeo: mantiene la postura y
 *                                 evita gastar el ki que sostiene el bonus.
 *
 * Nota: como PM_CheckBoost no llega a ejecutarse durante el forcejeo, el coste de
 * fatiga del boost (bg_pmove.c:821) tampoco se aplica. Mantenerlo apretado adentro
 * es gratis.
 */

/* g_usermissile.c exige plCurrent > 1 para que el boost aporte potencia. Esto
 * refleja una constante del MOTOR (bg_pmove.c:705), no una preferencia de diseno
 * -- se queda como codigo, no pasa a .cfg (T4.1). */
#define BOTLITE_STRUGGLE_KI_FLOOR		1

qboolean BotLite_IsStruggling( gentity_t *bot ) {
	if ( !bot || !bot->client ) {
		return qfalse;
	}
	return ( bot->client->ps.bitFlags & isStruggling ) ? qtrue : qfalse;
}

static qboolean BotLite_BeamIsActive( gentity_t *bot ) {
	if ( !bot || !bot->client ) {
		return qfalse;
	}

	switch ( bot->client->ps.weaponstate ) {
	case WEAPON_FIRING:
	case WEAPON_GUIDING:
	case WEAPON_CHARGING:
	case WEAPON_ALTFIRING:
	case WEAPON_ALTGUIDING:
	case WEAPON_ALTCHARGING:
		return qtrue;
	default:
		return qfalse;
	}
}

/* T4.1: el umbral por skill ahora vive en cada profile (struggleCommitPct),
 * en vez de un #define fijo por skill. */
static int BotLite_StruggleCommitThreshold( int skill ) {
	const botlite_profile_t *profile;

	profile = BotLite_GetProfile( skill );
	return profile ? profile->struggleCommitPct : 50;
}

/*
 * Se ejecuta mientras el bot carga o dispara un haz, ANTES de que exista choque.
 * Engancha el boost para dejar el flag puesto por si aparece un forcejeo.
 */
void BotLite_StrugglePrepare( gentity_t *bot, int clientNum ) {
	botlite_info_t *info;

	if ( !bot || !bot->client ) {
		return;
	}
	if ( clientNum < 0 || clientNum >= level.maxclients ) {
		return;
	}
	if ( !BotLite_BeamIsActive( bot ) ) {
		return;
	}

	info = &g_botlite[clientNum];
	if ( BotLite_KiPercent( clientNum ) < BotLite_StruggleCommitThreshold( info->skill ) ) {
		return;
	}
	/* Enganchar el boost cuesta fatiga (bg_pmove.c:821); dentro del forcejeo no. */
	if ( !BotLite_StaminaAllowsSpend( clientNum, BOTLITE_SPEND_CHEAP ) ) {
		return;
	}

	BotLite_EA_Button( bot, BUTTON_BOOST );
	BotLite_EA_AllowBoostWithAttack( bot );
}

/*
 * Se ejecuta ya dentro del forcejeo. Devuelve qtrue si tomo el control del frame.
 */
qboolean BotLite_RunStruggle( gentity_t *bot, int clientNum, const botlite_snapshot_t *snapshot ) {
	botlite_info_t *info;
	int enemyBeam;

	if ( !bot || !bot->client ) {
		return qfalse;
	}
	if ( clientNum < 0 || clientNum >= level.maxclients ) {
		return qfalse;
	}

	info = &g_botlite[clientNum];

	if ( !BotLite_IsStruggling( bot ) ) {
		if ( info->runtime.struggleStartTime ) {
			BotLite_DebugLog( bot, va( "Struggle ended after %dms beam=%d ki=%d%%",
				level.time - info->runtime.struggleStartTime,
				bot->client->ps.attackPowerCurrent,
				BotLite_KiPercent( clientNum ) ) );
			info->runtime.struggleStartTime = 0;
		}
		return qfalse;
	}

	if ( !info->runtime.struggleStartTime ) {
		info->runtime.struggleStartTime = level.time;
		enemyBeam = ( snapshot && snapshot->target && snapshot->target->client )
			? snapshot->target->client->ps.attackPowerCurrent : 0;
		BotLite_DebugLog( bot, va( "Struggle started beam=%d enemyBeam=%d ki=%d%% boosted=%d",
			bot->client->ps.attackPowerCurrent,
			enemyBeam,
			BotLite_KiPercent( clientNum ),
			( bot->client->ps.bitFlags & usingBoost ) ? 1 : 0 ) );
	}

	if ( snapshot && snapshot->target && snapshot->target->client ) {
		BotLite_FaceTarget( bot, snapshot->target );
	}

	/* El flag de boost ya quedo congelado al empezar el choque, asi que esto no
	 * puede cambiar el resultado; se mantiene apretado para no soltarlo si el
	 * forcejeo termina en este mismo frame, y porque adentro no cuesta fatiga. */
	if ( bot->client->ps.powerLevel[plCurrent] > BOTLITE_STRUGGLE_KI_FLOOR ) {
		BotLite_EA_Button( bot, BUTTON_BOOST );
		BotLite_EA_AllowBoostWithAttack( bot );
	}

	/* Lo unico que el bot todavia controla es no gastar el ki que sostiene el
	 * bonus: se queda quieto y no dispara ni se teletransporta. */
	return qtrue;
}
