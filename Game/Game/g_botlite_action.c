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
