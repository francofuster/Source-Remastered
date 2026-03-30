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
	BotLite_DebugLog( bot, "Face target" );
	VectorCopy( target->client->ps.origin, aimPos );
	aimPos[2] += BOTLITE_VIEW_HEIGHT;
	BotLite_FacePosition( bot, aimPos );
}


void BotLite_SetLockOn( gentity_t *bot, gentity_t *target ) {
	BotLite_DebugLog( bot, "Set lock-on" );
	if ( !bot || !bot->client || !BotLite_TargetIsValid( bot, target ) ) {
		return;
	}
	bot->client->ps.lockedTarget = target->s.number + 1;
	bot->client->ps.lockedPlayer = &target->client->ps;
	bot->client->ps.lockedPosition = &target->r.currentOrigin;
	target->client->ps.bitFlags |= isTargeted;
}


