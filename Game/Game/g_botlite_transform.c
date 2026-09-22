#include "g_local.h"
#include "g_botlite.h"

/*
 * ============================================================================
 * Control de transformacion (T2.3)
 * ============================================================================
 *
 * Asimetria del motor, verificada en g_tiers.c:62-97:
 *
 *   SUBIR encadena. El while(1) de checkTier vuelve a intentar con continue
 *   dentro del MISMO frame, asi que mientras keyTierUp este puesto y se cumplan
 *   los requisitos, el bot salta de tier 0 al maximo del personaje de una vez.
 *   No se puede dosificar la subida por tiempo de pulsacion.
 *
 *   BAJAR es de a uno. La rama de descenso hace break, y deja tmTransform = -1,
 *   que bloquea checkTier el frame siguiente.
 *
 * Como la granularidad de subida no es controlable, lo que se dosifica es el
 * MOMENTO: el bot ya no se transforma al ver al rival, sino cuando el combate lo
 * justifica. Y baja por decision propia antes de que el sustain lo fuerce, porque
 * la des-transformacion forzada llega en el peor momento posible.
 */

/* Margen sobre el minimo de sustain, en centesimos: 115 = alerta al 115%. */
#define BOTLITE_TIER_SUSTAIN_MARGIN_PCT		115

/*
 * Fase 6.7 -- cuanto hay que SOSTENER el boton de transformacion.
 *
 * Mismo problema que tenia el zanzoken, por un camino distinto. checkTier no
 * corre todos los frames: PM_CheckTransform (bg_pmove.c:587-595) emite
 * EV_TIERCHECK una vez cada 300ms, y g_active.c:405 recien ahi llama a checkTier.
 * Por otro lado keyTierUp se pone y se saca dentro del mismo PM_CheckPowerLevel
 * (bg_pmove.c:692-715): en cuanto el boton se suelta, el flag se borra.
 *
 * O sea que un toque de un frame (50ms) solo sirve si justo cae en el tick de los
 * 300ms. En el log del 22-09: 23 pulsaciones, 0 transformaciones, y los 1101
 * snapshots con tier=0. Sosteniendo por encima de 300ms el tick cae dentro si o
 * si.
 */
#define BOTLITE_TIER_HOLD_MS				450

/* Vida propia (en % del maximo) por debajo de la cual conviene escalar. */
#define BOTLITE_TIER_ESCALATE_HEALTH_PCT	75

static qboolean BotLite_ValueBelowMargin( int value, int minimum ) {
	if ( minimum <= 0 ) {
		return qfalse;
	}
	return ( value < ( minimum * BOTLITE_TIER_SUSTAIN_MARGIN_PCT ) / 100 ) ? qtrue : qfalse;
}

/*
 * El tier actual esta por caer: conviene bajar a voluntad antes de que el motor
 * lo haga por nosotros (g_tiers.c:88-97).
 */
static qboolean BotLite_TierSustainAtRisk( gentity_t *bot ) {
	tierConfig_g *cfg;
	playerState_t *ps;
	int tier;

	ps = &bot->client->ps;
	tier = ps->powerLevel[plTierCurrent];
	if ( tier <= 0 || tier >= 8 ) {
		return qfalse;
	}

	cfg = &bot->client->tiers[tier];
	if ( !cfg->exists || cfg->permanent ) {
		return qfalse;
	}

	if ( BotLite_ValueBelowMargin( ps->powerLevel[plCurrent], cfg->sustainCurrent ) ) {
		return qtrue;
	}
	if ( BotLite_ValueBelowMargin( ps->powerLevel[plHealth], cfg->sustainHealth ) ) {
		return qtrue;
	}
	if ( BotLite_ValueBelowMargin( ps->powerLevel[plFatigue], cfg->sustainFatigue ) ) {
		return qtrue;
	}
	if ( BotLite_ValueBelowMargin( ps->powerLevel[plMaximum], cfg->sustainMaximum ) ) {
		return qtrue;
	}
	return qfalse;
}

/*
 * Fase 6.4 -- no escalar a un tier que no se va a poder sostener.
 *
 * El motor baja de tier solo en cuanto alguno de los valores cae por debajo del
 * sustain del tier actual (g_tiers.c:88-97). Si el bot sube estando justo en el
 * limite, la caida forzada llega enseguida y vuelve a intentarlo: en el log del
 * 22-09 el tier alterna 3 -> 4 -> 3 -> 4 un snapshot por vez, con 4637 intentos
 * de escalada en una sola sesion.
 *
 * El caso concreto era goku tier4, que pide sustainFatigue 8000 y ademas drena
 * fatiga mientras esta activo (effectFatigue -15). Subir con la fatiga en 8001
 * garantiza la caida; hay que subir con holgura.
 */
static qboolean BotLite_ValueAboveMargin( int value, int minimum, int marginPct ) {
	if ( minimum <= 0 ) {
		return qtrue;
	}
	if ( marginPct <= 0 ) {
		marginPct = 100;
	}
	return ( value >= ( minimum * marginPct ) / 100 ) ? qtrue : qfalse;
}

static qboolean BotLite_TierEscalationViable( gentity_t *bot, int marginPct ) {
	tierConfig_g *next;
	playerState_t *ps;
	int tier;

	ps = &bot->client->ps;
	tier = ps->powerLevel[plTierCurrent];
	if ( tier < 0 || tier >= 7 ) {
		return qfalse;
	}
	next = &bot->client->tiers[tier + 1];
	if ( !next->exists ) {
		return qfalse;
	}

	/* Requisitos duros del motor (g_tiers.c:67-70). Sin esto el boton no hace
		 * nada y el bot se queda apretandolo. */
	if ( ps->powerLevel[plCurrent] < next->requirementCurrent ) {
		return qfalse;
	}
	if ( ps->powerLevel[plFatigue] < next->requirementFatigue ) {
		return qfalse;
	}
	if ( ps->powerLevel[plHealth] < next->requirementHealth ) {
		return qfalse;
	}
	if ( ps->powerLevel[plMaximum] < next->requirementMaximum ) {
		return qfalse;
	}

	/* Sustain CON MARGEN: entrar justo al limite es entrar para caerse. */
	if ( !BotLite_ValueAboveMargin( ps->powerLevel[plCurrent], next->sustainCurrent, marginPct ) ) {
		return qfalse;
	}
	if ( !BotLite_ValueAboveMargin( ps->powerLevel[plHealth], next->sustainHealth, marginPct ) ) {
		return qfalse;
	}
	if ( !BotLite_ValueAboveMargin( ps->powerLevel[plFatigue], next->sustainFatigue, marginPct ) ) {
		return qfalse;
	}
	if ( !BotLite_ValueAboveMargin( ps->powerLevel[plMaximum], next->sustainMaximum, marginPct ) ) {
		return qfalse;
	}
	return qtrue;
}

static qboolean BotLite_HasHigherTier( gentity_t *bot ) {

	int tier;

	tier = bot->client->ps.powerLevel[plTierCurrent];
	if ( tier < 0 || tier >= 7 ) {
		return qfalse;
	}
	return bot->client->tiers[tier + 1].exists ? qtrue : qfalse;
}

/*
 * Que justifica escalar. La idea es que el bot no abra el combate en su forma
 * final: que la transformacion sea una respuesta a la pelea, no un ritual.
 */
static qboolean BotLite_ShouldEscalateTier( gentity_t *bot, int clientNum, const botlite_snapshot_t *snapshot, const botlite_profile_t *profile ) {
	botlite_info_t *info;
	int healthPct;

	info = &g_botlite[clientNum];

	if ( !snapshot || !snapshot->hasTarget ) {
		/* No se reinicia el reloj: perder el objetivo un instante (una esquina, un
			 * respawn del rival) no deberia borrar que el bot viene peleando hace rato.
			 * El reset de verdad ocurre al morir, en BotLite_ResetCoreRuntimeState. */
		return qfalse;
	}
	if ( info->ranged.tierEngagedSince == 0 ) {
		info->ranged.tierEngagedSince = level.time;
	}

	/* El rival esta por encima: igualar o superar. */
	if ( snapshot->targetTier > snapshot->botTier ) {
		return qtrue;
	}

	/* La pelea se esta poniendo cara. */
	if ( snapshot->botKiMax > 0 ) {
		healthPct = (int)( ( (float)snapshot->botHealth * 100.0f ) / (float)snapshot->botKiMax );
		if ( healthPct < BOTLITE_TIER_ESCALATE_HEALTH_PCT ) {
			return qtrue;
		}
	}

	/*
	 * Fase 6.7 -- pedido explicito: "quiero que se pueda transformar
	 * independientemente del tier del target".
	 *
	 * Los disparadores de arriba miran al rival o miran a estar perdiendo. Este
	 * mira solo al propio bot: si lleva un rato peleando, escalar se justifica por
	 * si solo. Quien decide si de verdad se puede sigue siendo
	 * BotLite_TierEscalationViable, que exige los requisitos del motor y el
	 * sustain del tier siguiente con margen -- o sea que esto no lo empuja a subir
	 * a un tier que no pueda sostener.
	 *
	 * Fase 6.5 -- tercer disparador: combate sostenido.
	 *
	 * Con solo los dos de arriba, un rival que no se transforma y no llega a
	 * bajarle la vida del 75%% deja al bot en tier 0 toda la partida. Se vio en
	 * juego: 436 snapshots, todos con tier=0, contra un rival tambien en tier 0.
	 * Que la transformacion sea una respuesta al combate no quiere decir que
	 * tenga que ser una respuesta a estar perdiendo.
	 *
	 * No es un ritual de apertura: pide pelea sostenida contra el mismo objetivo,
	 * y la viabilidad con margen (BotLite_TierEscalationViable) sigue decidiendo
	 * si de verdad se puede.
	 */
	if ( profile && profile->tierEscalateCombatMs > 0 &&
		 level.time - info->ranged.tierEngagedSince >= profile->tierEscalateCombatMs ) {
		return qtrue;
	}

	return qfalse;
}

/*
 * Devuelve qtrue si tomo el control del frame.
 */
qboolean BotLite_RunTransformControl( gentity_t *bot, int clientNum, const botlite_snapshot_t *snapshot ) {
	botlite_info_t *info;
	const botlite_combat_policy_t *policy;
	const botlite_profile_t *profile;

	if ( !bot || !bot->client ) {
		return qfalse;
	}

	info = &g_botlite[clientNum];
	policy = BotLite_GetCombatPolicy( info->skill );

	/* T1.2: skill sin permiso no toca tiers ni para subir ni para bajar. */
	if ( !policy || !policy->needsInitialTransform ) {
		info->ranged.didInitialTransform = qtrue;
		return qfalse;
	}

	/* Sostenido en curso: mantener el boton y la direccion hasta cumplir la
		 * ventana, para que el tick de checkTier caiga dentro. */
	if ( level.time < info->ranged.tierHoldUntil ) {
		BotLite_ClearLock( bot );
		BotLite_EA_Button( bot, BUTTON_POWERLEVEL );
		BotLite_EA_MoveForward( bot, info->ranged.tierHoldDirection );
		return qtrue;
	}

	/* Animacion en curso: checkTier ignora todo mientras tmTransform != 0. */
	if ( bot->client->ps.timers[tmTransform] != 0 ) {
		BotLite_ClearLock( bot );
		return qtrue;
	}

	/* Confirmacion de que la transformacion aterrizo de verdad. Sin esto no habia
		 * forma de distinguir "aprieta el boton" de "se transforma". */
	if ( info->ranged.tierLastSeen != bot->client->ps.powerLevel[plTierCurrent] ) {
		if ( info->ranged.tierLastSeen >= 0 ) {
			BotLite_DebugLog( bot, va( "Tier cambio: %d -> %d",
				info->ranged.tierLastSeen, bot->client->ps.powerLevel[plTierCurrent] ) );
		}
		info->ranged.tierLastSeen = bot->client->ps.powerLevel[plTierCurrent];
	}

	/* Bajar a voluntad antes de que el sustain falle. La forzada deja al bot
	 * vulnerable justo cuando ya estaba en problemas de recursos. */
	if ( BotLite_TierSustainAtRisk( bot ) ) {
		BotLite_DebugLog( bot, va( "Tier step down (sustain at risk) tier=%d",
			bot->client->ps.powerLevel[plTierCurrent] ) );
		info->ranged.tierHoldUntil = level.time + BOTLITE_TIER_HOLD_MS;
		info->ranged.tierHoldDirection = -127;
		BotLite_EA_Button( bot, BUTTON_POWERLEVEL );
		BotLite_EA_MoveBack( bot, 127 );
		return qtrue;
	}

	if ( !BotLite_HasHigherTier( bot ) ) {
		info->ranged.didInitialTransform = qtrue;
		return qfalse;
	}

	profile = info->profile ? info->profile : BotLite_GetProfile( info->skill );

	if ( !BotLite_ShouldEscalateTier( bot, clientNum, snapshot, profile ) ) {
		/* Sin motivo todavia. No se marca didInitialTransform: la puerta queda
		 * abierta para escalar mas adelante en la misma pelea. */
		return qfalse;
	}

	/*
	 * Los dos retornos qfalse de aca abajo son deliberados: NO consumen el frame.
	 *
	 * Antes esta funcion devolvia qtrue en cuanto habia motivo para escalar, sin
	 * mirar si la escalada era posible. Con los recursos por debajo del requisito
	 * el boton no hacia nada y el bot se quedaba apretandolo todos los frames:
	 * 4637 intentos en una sesion, sin atacar, sin boost y sin cargar ki -- o sea
	 * sin poder juntar nunca los recursos que le faltaban. Reportado en juego como
	 * "dejan de usar ki boost y transformarse aun teniendo ki".
	 *
	 * Cediendo el frame, el bot vuelve a pelear y a cargar, que es lo unico que
	 * puede acercarlo al siguiente tier.
	 */
	if ( level.time < info->ranged.tierNextAttemptTime ) {
		return qfalse;
	}
	if ( !BotLite_TierEscalationViable( bot, profile ? profile->tierEscalateMarginPct : 130 ) ) {
		if ( level.time >= info->ranged.tierLastLogTime ) {
			info->ranged.tierLastLogTime = level.time + 5000;
			BotLite_DebugLog( bot, va( "Tier escalate postergado: sin margen tier=%d ki=%d%% fatiga=%d%%",
				bot->client->ps.powerLevel[plTierCurrent],
				BotLite_KiPercent( clientNum ),
				BotLite_StaminaPercent( clientNum ) ) );
		}
		return qfalse;
	}

	info->ranged.tierNextAttemptTime = level.time + ( profile ? profile->transformIntervalMs : 20000 );
	info->ranged.tierHoldUntil = level.time + BOTLITE_TIER_HOLD_MS;
	info->ranged.tierHoldDirection = 127;
	BotLite_ClearLock( bot );
	BotLite_DebugLog( bot, va( "Tier escalate from=%d targetTier=%d hp=%d",
		bot->client->ps.powerLevel[plTierCurrent],
		snapshot ? snapshot->targetTier : -1,
		bot->client->ps.powerLevel[plHealth] ) );
	BotLite_EA_Button( bot, BUTTON_POWERLEVEL );
	BotLite_EA_MoveForward( bot, 127 );
	return qtrue;
}
