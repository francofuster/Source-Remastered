#include "g_local.h"
#include "g_botlite.h"

/*
 * ============================================================================
 * FASE 6.2 -- Zanzoken sostenido
 * ============================================================================
 *
 * Reportado en juego, tres veces seguidas: "sigo viendo que no hace el sanzoken".
 * Las correcciones anteriores (T2.5) fueron todas sobre CUANDO decidirlo. El
 * problema real era COMO se ejecuta.
 *
 * Como funciona de verdad PM_CheckZanzoken (bg_pmove.c:307-350):
 *
 *   1) Al soltar el boton:      tmZanzoken = -1     (queda rearmado)
 *   2) Al apretarlo con -1:     usingZanzoken = on
 *                               tmZanzoken = plFatigue / 93.62 + stZanzokenDistance
 *                               se cobra plMaximum * 0.12 * stZanzokenCost de fatiga
 *   3) Mientras tmZanzoken > 0: VectorNormalize(velocity)
 *                               VectorScale(velocity, speed)
 *   4) usingZanzoken con tmZanzoken <= 0 y nada delante: PM_StopZanzoken(),
 *      que hace VectorClear(velocity).
 *
 * De ahi salen las dos condiciones que el bot no cumplia:
 *
 * A) HAY QUE SOSTENER EL BOTON. Un toque de un frame entra por el paso 2, y al
 *    frame siguiente -- con el boton ya suelto -- el paso 1 pone tmZanzoken = -1,
 *    lo que dispara el paso 4 y frena en seco. Se pagaba el costo completo de
 *    fatiga para desplazarse practicamente nada. Eso es exactamente lo que hacia
 *    BotLite_EA_Button(bot, BUTTON_TELEPORT) suelto, porque BotLite_ActionReset
 *    limpia la accion al inicio de cada think.
 *
 * B) HACE FALTA UNA DIRECCION. El paso 3 no teletransporta a ningun lado: NORMALIZA
 *    LA VELOCIDAD QUE YA SE TIENE y la escala. Con el bot quieto, esa velocidad es
 *    cero, normalizar cero da cero y escalar cero da cero. Dos de los tres puntos
 *    donde el bot apretaba teleport (escape defensivo y esquive de proyectil) no
 *    mandaban ningun movimiento.
 *
 * Nota sobre los recursos, porque es facil confundirlos: la DURACION sale de la
 * fatiga (plFatigue / 93.62) y el costo tambien es fatiga; lo que escala con ki es
 * la VELOCIDAD (plCurrent / 13.1, bg_pmove.c:330). O sea que con la fatiga en el
 * piso el zanzoken dura nada aunque sobre el ki, y con el ki bajo dura igual pero
 * llega mucho menos lejos.
 */

/* Topes de cordura por si un stat de personaje da un valor absurdo. */
#define BOTLITE_ZANZOKEN_MIN_MS		120
/* Tope alto a proposito: solo esta para atajar un stat corrupto, no para
 * recortar duraciones legitimas. stats[stZanzokenDistance] es zanzokenDistance
 * del tier por 500 (g_tiers.c:28), asi que con 1.6 ya son 800ms de base mas los
 * ~350 que aporta la fatiga llena. Un tope de 1500 estaba cortando zanzokens
 * validos -- se vio en el log, varios arrancaban exactamente en 1500ms. */
#define BOTLITE_ZANZOKEN_MAX_MS		3000

/*
 * Misma formula que bg_pmove.c:344. Se recalcula aca en vez de leer tmZanzoken
 * porque hay que saber cuanto va a durar ANTES de apretar el boton.
 */
int BotLite_ZanzokenDurationMs( gentity_t *bot ) {
	int duration;

	if ( !bot || !bot->client ) {
		return BOTLITE_ZANZOKEN_MIN_MS;
	}

	duration = (int)( (float)bot->client->ps.powerLevel[plFatigue] / 93.62f );
	duration += bot->client->ps.stats[stZanzokenDistance];

	if ( duration < BOTLITE_ZANZOKEN_MIN_MS ) {
		duration = BOTLITE_ZANZOKEN_MIN_MS;
	}
	if ( duration > BOTLITE_ZANZOKEN_MAX_MS ) {
		duration = BOTLITE_ZANZOKEN_MAX_MS;
	}
	return duration;
}

/*
 * Arranca un zanzoken sostenido en la direccion pedida.
 *
 * Los tres valores son comandos de movimiento (-127..127) relativos a la vista,
 * no un vector del mundo: quien llama ya oriento al bot hacia donde quiere ir.
 * Al menos uno tiene que ser distinto de cero o el zanzoken no desplaza nada.
 */
void BotLite_StartZanzoken( gentity_t *bot, int clientNum, int moveForward, int moveRight, int moveUp ) {
	botlite_info_t *info;
	int duration;

	if ( !bot || !bot->client || clientNum < 0 || clientNum >= BOTLITE_MAX_BOTS ) {
		return;
	}
	if ( moveForward == 0 && moveRight == 0 && moveUp == 0 ) {
		/* Sin direccion seria pagar la fatiga para quedarse en el lugar. */
		moveForward = 127;
	}

	info = &g_botlite[clientNum];
	duration = BotLite_ZanzokenDurationMs( bot );

	info->melee.zanzokenHoldUntil = level.time + duration;
	info->melee.zanzokenMoveForward = moveForward;
	info->melee.zanzokenMoveRight = moveRight;
	info->melee.zanzokenMoveUp = moveUp;

	BotLite_DebugLog( bot, va( "Zanzoken start: %dms fwd=%d right=%d up=%d fatiga=%d%%",
		duration, moveForward, moveRight, moveUp, BotLite_StaminaPercent( clientNum ) ) );

	/* El primer frame del sostenido se emite ya mismo. */
	BotLite_EA_Button( bot, BUTTON_TELEPORT );
	BotLite_EA_MoveForward( bot, moveForward );
	BotLite_EA_MoveRight( bot, moveRight );
	BotLite_EA_MoveUp( bot, moveUp );
}

/*
 * Sostiene el zanzoken en curso. Tiene que correr antes que cualquier otra
 * decision del think loop: si otro modulo se lleva el frame, el boton se suelta
 * y el motor frena el desplazamiento a mitad de camino.
 *
 * Devuelve qtrue mientras el sostenido esta activo.
 */
qboolean BotLite_RunZanzokenHold( gentity_t *bot, int clientNum, const botlite_snapshot_t *snapshot ) {
	botlite_info_t *info;

	if ( !bot || !bot->client || clientNum < 0 || clientNum >= BOTLITE_MAX_BOTS ) {
		return qfalse;
	}

	info = &g_botlite[clientNum];
	if ( info->melee.zanzokenHoldUntil <= level.time ) {
		return qfalse;
	}

	/* Estados donde el motor ya lo corto o lo va a rechazar: soltar y no insistir.
		 * PM_CheckZanzoken devuelve temprano con usingMelee/usingSoar/isPreparing, y
		 * con plFatigue <= 1 llama directamente a PM_StopZanzoken. */
	if ( snapshot && ( snapshot->botInMelee || snapshot->botDisabled || snapshot->botFrozen ||
			snapshot->botStruggling ) ) {
		info->melee.zanzokenHoldUntil = 0;
		BotLite_DebugLog( bot, "Zanzoken cortado: estado del motor lo bloquea" );
		return qfalse;
	}
	if ( bot->client->ps.powerLevel[plFatigue] <= 1 ) {
		info->melee.zanzokenHoldUntil = 0;
		BotLite_DebugLog( bot, "Zanzoken cortado: sin fatiga" );
		return qfalse;
	}

	BotLite_EA_Button( bot, BUTTON_TELEPORT );
	BotLite_EA_MoveForward( bot, info->melee.zanzokenMoveForward );
	BotLite_EA_MoveRight( bot, info->melee.zanzokenMoveRight );
	BotLite_EA_MoveUp( bot, info->melee.zanzokenMoveUp );
	return qtrue;
}
