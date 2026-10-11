#include "g_local.h"
#include "g_botlite.h"

static int BotLite_ClampMove( int value ) {
	if ( value > 127 ) {
		return 127;
	}
	if ( value < -127 ) {
		return -127;
	}
	return value;
}


static int BotLite_SanitizeButtons( int buttons, qboolean allowBoostWithAttack ) {
	if ( buttons & BUTTON_TELEPORT ) {
		buttons &= ~( BUTTON_BOOST | BUTTON_BLOCK | BUTTON_ATTACK | BUTTON_ALT_ATTACK | BUTTON_POWERLEVEL );
		return buttons;
	}
	if ( buttons & BUTTON_POWERLEVEL ) {
		buttons &= ~( BUTTON_BOOST | BUTTON_BLOCK | BUTTON_ATTACK | BUTTON_ALT_ATTACK );
		return buttons;
	}
	if ( buttons & BUTTON_BLOCK ) {
		buttons &= ~( BUTTON_BOOST | BUTTON_ATTACK | BUTTON_ALT_ATTACK );
		return buttons;
	}
	if ( buttons & ( BUTTON_ATTACK | BUTTON_ALT_ATTACK ) ) {
		/* Excepcion deliberada: boost + ataque es legal y necesario. Es lo unico
		 * que suma potencia en un forcejeo de haces (g_usermissile.c:443) y ademas
		 * duplica la velocidad de carga (bg_pmove.c:2834). Solo se habilita cuando
		 * el modulo de forcejeo lo pide, para no alterar el resto del combate. */
		if ( !allowBoostWithAttack ) {
			buttons &= ~BUTTON_BOOST;
		}
	}
	return buttons;
}

void BotLite_ActionReset( int clientNum, int serverTime ) {
	botlite_info_t *info;

	if ( clientNum < 0 || clientNum >= level.maxclients ) {
		return;
	}

	info = &g_botlite[clientNum];
	memset( &info->action, 0, sizeof( info->action ) );
}

/*
 * Con el lock-on puesto el pmove apunta la vista al rival (PM_UpdateViewAngles),
 * pero mueve segun la vista con la que arranca el paso: la que pidio el bot.
 * Cuando el bot miraba hacia otro lado para ir a algun lado (alejarse mientras
 * el rival se recupera de un crash, un escondite, la busqueda) se desplazaba
 * hacia atras o de costado mirando al rival y con la animacion de avanzar.
 * Aca el movimiento pedido pasa a la vista del lock: el bot mira al rival, va
 * hacia el mismo lado que queria, y la animacion es la del movimiento real
 * (retroceder, de costado). De paso el forwardmove positivo vuelve a significar
 * acercarse, que es lo que PM_Melee entiende por embestir.
 */
static void BotLite_AlignMoveToLock( gentity_t *bot, usercmd_t *cmd ) {
	playerState_t	*ps;
	gentity_t		*target;
	vec3_t			dir, lockAngles, viewAngles, f0, r0, u0, f1, r1, u1, wish;
	qboolean		flying;

	ps = &bot->client->ps;
	if ( ps->lockedTarget <= 0 || ps->lockedTarget > MAX_CLIENTS ) {
		return;
	}
	if ( !cmd->forwardmove && !cmd->rightmove && !cmd->upmove ) {
		return;
	}
	/* sin aim del lock (cargando ki) o sin movimiento propio (knockback, crash, congelado) */
	if ( ( ps->bitFlags & ( usingAlter | isCrashed | isDead | isUnconcious ) ) || ps->timers[tmKnockback] || ps->timers[tmFreeze] ) {
		return;
	}
	target = &g_entities[ps->lockedTarget - 1];
	if ( !target->inuse || !target->client || target->client->ps.timers[tmZanzokenVanish] > 0 ) {
		return;
	}
	VectorSubtract( target->r.currentOrigin, ps->origin, dir );
	if ( VectorNormalize( dir ) < 1.0f ) {
		return;
	}
	vectoangles( dir, lockAngles );
	lockAngles[ROLL] = 0;
	if ( Q_fabs( AngleSubtract( lockAngles[YAW], ps->viewangles[YAW] ) ) < 10.0f
		&& Q_fabs( AngleSubtract( lockAngles[PITCH], ps->viewangles[PITCH] ) ) < 10.0f ) {
		return;
	}
	/* en el suelo el movimiento es horizontal: sin pitch, y el upmove queda como estaba */
	flying = ( ps->bitFlags & usingFlight ) ? qtrue : qfalse;
	VectorCopy( ps->viewangles, viewAngles );
	if ( !flying ) {
		viewAngles[PITCH] = 0;
		lockAngles[PITCH] = 0;
	}
	AngleVectors( viewAngles, f0, r0, u0 );
	VectorScale( f0, cmd->forwardmove, wish );
	VectorMA( wish, cmd->rightmove, r0, wish );
	if ( flying ) {
		VectorMA( wish, cmd->upmove, u0, wish );
	}
	AngleVectors( lockAngles, f1, r1, u1 );
	cmd->forwardmove = BotLite_ClampMove( (int)DotProduct( wish, f1 ) );
	cmd->rightmove = BotLite_ClampMove( (int)DotProduct( wish, r1 ) );
	if ( flying ) {
		cmd->upmove = BotLite_ClampMove( (int)DotProduct( wish, u1 ) );
	}
	SetClientViewAngle( bot, lockAngles );
}

void BotLite_ActionCommit( gentity_t *bot, int clientNum, int serverTime ) {
	botlite_info_t *info;
	usercmd_t *cmd;

	if ( !bot || !bot->client ) {
		return;
	}

	info = &g_botlite[clientNum];
	cmd = &bot->client->pers.cmd;
	memset( cmd, 0, sizeof( *cmd ) );
	cmd->serverTime = serverTime;
	cmd->forwardmove = BotLite_ClampMove( info->action.forwardmove );
	cmd->rightmove = BotLite_ClampMove( info->action.rightmove );
	cmd->upmove = BotLite_ClampMove( info->action.upmove );
	cmd->buttons = BotLite_SanitizeButtons( info->action.buttons, info->action.allowBoostWithAttack );
	if ( info->action.weaponOverride ) {
		cmd->weapon = info->action.weapon;
	}
	BotLite_AlignMoveToLock( bot, cmd );
}

void BotLite_EA_MoveForward( gentity_t *bot, int value ) {
	botlite_action_t *action;

	if ( !bot || !bot->client ) {
		return;
	}

	action = &g_botlite[bot->s.number].action;
	action->forwardmove = BotLite_ClampMove( value );
}

void BotLite_EA_MoveBack( gentity_t *bot, int value ) {
	BotLite_EA_MoveForward( bot, -value );
}

void BotLite_EA_MoveRight( gentity_t *bot, int value ) {
	botlite_action_t *action;

	if ( !bot || !bot->client ) {
		return;
	}

	action = &g_botlite[bot->s.number].action;
	action->rightmove = BotLite_ClampMove( value );
}

void BotLite_EA_MoveUp( gentity_t *bot, int value ) {
	botlite_action_t *action;

	if ( !bot || !bot->client ) {
		return;
	}

	action = &g_botlite[bot->s.number].action;
	action->upmove = BotLite_ClampMove( value );
}

void BotLite_EA_Button( gentity_t *bot, int buttonMask ) {
	botlite_action_t *action;

	if ( !bot || !bot->client ) {
		return;
	}

	action = &g_botlite[bot->s.number].action;
	action->buttons |= buttonMask;
}

void BotLite_EA_AllowBoostWithAttack( gentity_t *bot ) {
	if ( !bot || !bot->client ) {
		return;
	}
	g_botlite[bot->s.number].action.allowBoostWithAttack = qtrue;
}

void BotLite_EA_SetWeapon( gentity_t *bot, int weapon ) {
	botlite_action_t *action;

	if ( !bot || !bot->client ) {
		return;
	}

	action = &g_botlite[bot->s.number].action;
	action->weapon = weapon;
	action->weaponOverride = qtrue;
}
