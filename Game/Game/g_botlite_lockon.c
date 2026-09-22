#include "g_local.h"
#include "g_botlite.h"

static void BotLite_FacePosition( gentity_t *bot, const vec3_t targetPos );

void BotLite_ClearLock( gentity_t *bot ) {
	if ( !bot || !bot->client ) {
		return;
	}
	if ( bot->client->ps.lockedTarget > 0 && bot->client->ps.lockedPlayer ) {
		bot->client->ps.lockedPlayer->bitFlags &= ~isTargeted;
	}
	bot->client->ps.lockedTarget = 0;
	bot->client->ps.lockedPlayer = NULL;
	bot->client->ps.lockedPosition = NULL;
}


void BotLite_ApplyViewAngles( gentity_t *bot, const vec3_t angles ) {
	vec3_t fixedAngles;

	VectorCopy( angles, fixedAngles );
	fixedAngles[ROLL] = 0;
	SetClientViewAngle( bot, fixedAngles );
	VectorCopy( fixedAngles, bot->r.currentAngles );
}


static void BotLite_FacePosition( gentity_t *bot, const vec3_t targetPos ) {
	vec3_t dir;
	vec3_t angles;

	VectorSubtract( targetPos, bot->client->ps.origin, dir );
	vectoangles( dir, angles );
	angles[ROLL] = 0;
	BotLite_ApplyViewAngles( bot, angles );
}


void BotLite_FaceTarget( gentity_t *bot, gentity_t *target ) {
	vec3_t aimPos;
	VectorCopy( target->client->ps.origin, aimPos );
	aimPos[2] += BOTLITE_VIEW_HEIGHT;
	BotLite_FacePosition( bot, aimPos );
}


/*
 * Fase 6.3 -- mirar al objetivo pero sin inclinarse.
 *
 * En PM_FlyMove el movimiento sale de los vectores de la VISTA:
 *   wishvel = forward*forwardmove + right*rightmove + up*upmove   (bg_pmove.c:1300)
 * o sea que con pitch != 0 el "adelante" tiene componente vertical y el bot llega
 * al duelo picando o trepando. Con el pitch en 0, forward queda horizontal y up
 * queda alineado con el eje Z del mundo: la altura se corrige con upmove, que es
 * lo que deja la llegada plana.
 *
 * maxPitch = 0 es completamente horizontal; valores mayores permiten inclinarse
 * hasta ese tope.
 */
void BotLite_FaceTargetLeveled( gentity_t *bot, gentity_t *target, float maxPitch ) {
	vec3_t aimPos;
	vec3_t dir;
	vec3_t angles;

	if ( !bot || !bot->client || !target || !target->client ) {
		return;
	}

	VectorCopy( target->client->ps.origin, aimPos );
	aimPos[2] += BOTLITE_VIEW_HEIGHT;
	VectorSubtract( aimPos, bot->client->ps.origin, dir );
	vectoangles( dir, angles );
	angles[ROLL] = 0;

	if ( angles[PITCH] > 180.0f ) {
		angles[PITCH] -= 360.0f;
	}
	if ( angles[PITCH] > maxPitch ) {
		angles[PITCH] = maxPitch;
	}
	if ( angles[PITCH] < -maxPitch ) {
		angles[PITCH] = -maxPitch;
	}

	BotLite_ApplyViewAngles( bot, angles );
}

void BotLite_SetLockOn( gentity_t *bot, gentity_t *target ) {

	if ( !bot || !bot->client || !BotLite_TargetIsValid( bot, target ) ) {
		return;
	}
	/* Se llama cada frame: loguear solo el cambio real de objetivo, si no el
		 * log queda inservible (era el 96% de las lineas). */
	if ( bot->client->ps.lockedTarget != target->s.number + 1 ) {
		BotLite_DebugLog( bot, va( "Set lock-on -> %d", target->s.number ) );
	}
	bot->client->ps.lockedTarget = target->s.number + 1;
	bot->client->ps.lockedPlayer = &target->client->ps;
	bot->client->ps.lockedPosition = &target->r.currentOrigin;
	target->client->ps.bitFlags |= isTargeted;
}


