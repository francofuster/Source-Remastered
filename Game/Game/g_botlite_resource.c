#include "g_local.h"
#include "g_botlite.h"

/*
 * ============================================================================
 * Presupuesto de recursos del bot
 * ============================================================================
 *
 * Un unico lugar donde vive la politica de gasto. Cualquier accion que consuma
 * stamina (boost, zanzoken, cargas de melee, ataques de ki, transformacion)
 * consulta aca en vez de decidir por su cuenta; si no, con ~8 mecanicas distintas
 * gastando fatiga el conjunto se vuelve imposible de tunear.
 *
 * Mientras g_botStamina siga en 0 la fatiga del bot esta congelada al maximo por
 * PM_BotFatigueExempt (bg_pmove.c), y este modulo devuelve siempre COMFORTABLE:
 * se puede cablear en todo el codigo sin alterar el comportamiento actual.
 *
 * Notas de la formula de recuperacion (bg_pmove.c:636-648), relevantes para el
 * ciclo de recuperacion de T2.8:
 *   - statScale = 1 - (ki/kiMax), clamp [0.25,0.75] -> bajar el ki recupera hasta 3x
 *   - idleScale = 2.8 estando completamente quieto
 *   - fatigueScale tiene piso 0.15 -> cuanto mas agotado, mas lento recupera
 *   - recovery = 0 mientras usingAlter / isStruggling / usingSoar / isBreakingLimit
 */

/* T4.1: umbrales de clasificacion de stamina -- ahora en cada profile
 * (staminaComfortablePct/TightPct/CriticalPct), por defecto en defaults.cfg. */

/* T2.7/T2.8 -- etapas del ciclo de recuperacion, compartidas por ambas tareas. */
#define BOTLITE_RECOVER_STAGE_DISENGAGE	0
#define BOTLITE_RECOVER_STAGE_DRAIN		1
#define BOTLITE_RECOVER_STAGE_REST		2
#define BOTLITE_RECOVER_STAGE_RECHARGE	3

/*
 * Sin progreso de vida por mas que esto, se asume que plHealthPool esta vacio.
 * El pool se llena con el dano que UNO hace (bg_pmove.c:2479, 2594), asi que un
 * bot que viene perdiendo todos los intercambios no tiene nada que convertir, y
 * quedarse esperando la curacion es tiempo tirado.
 */
#define BOTLITE_RECOVER_HEAL_STALL_MS	2500

qboolean BotLite_RealStaminaEnabled( void ) {
	return g_botStamina.integer ? qtrue : qfalse;
}

int BotLite_StaminaPercent( int clientNum ) {
	gentity_t *bot;
	int maximum;

	if ( clientNum < 0 || clientNum >= level.maxclients ) {
		return 100;
	}

	bot = &g_entities[clientNum];
	if ( !bot->inuse || !bot->client ) {
		return 100;
	}

	maximum = bot->client->ps.powerLevel[plMaximum];
	if ( maximum <= 0 ) {
		return 100;
	}

	/* En float a proposito: g_powerlevelMaximum es un cvar de usuario y con un
		 * tope alto (>21M) la forma entera (valor * 100) desbordaria el int de 32 bits.
		 * Para un porcentaje la precision del float sobra. */
	return (int)( ( (float)bot->client->ps.powerLevel[plFatigue] * 100.0f ) / (float)maximum );
}

int BotLite_KiPercent( int clientNum ) {
	gentity_t *bot;
	int maximum;

	if ( clientNum < 0 || clientNum >= level.maxclients ) {
		return 100;
	}

	bot = &g_entities[clientNum];
	if ( !bot->inuse || !bot->client ) {
		return 100;
	}

	maximum = bot->client->ps.powerLevel[plMaximum];
	if ( maximum <= 0 ) {
		return 100;
	}

	return (int)( ( (float)bot->client->ps.powerLevel[plCurrent] * 100.0f ) / (float)maximum );
}

/* plHealth se regenera hacia plMaximum (bg_pmove.c:678), asi que plMaximum es
 * la referencia correcta de 'vida llena'. */
int BotLite_HealthPercent( int clientNum ) {
	gentity_t *bot;
	int maximum;

	if ( clientNum < 0 || clientNum >= level.maxclients ) {
		return 100;
	}

	bot = &g_entities[clientNum];
	if ( !bot->inuse || !bot->client ) {
		return 100;
	}

	maximum = bot->client->ps.powerLevel[plMaximum];
	if ( maximum <= 0 ) {
		return 100;
	}

	return (int)( ( (float)bot->client->ps.powerLevel[plHealth] * 100.0f ) / (float)maximum );
}

botlite_stamina_t BotLite_StaminaBudget( int clientNum ) {
	int percent;
	const botlite_profile_t *profile;

	/* Con la exencion puesta la fatiga no baja nunca: cualquier clasificacion
	 * distinta de COMFORTABLE seria mentira y frenaria acciones sin motivo. */
	if ( !BotLite_RealStaminaEnabled() ) {
		return BOTLITE_STAMINA_COMFORTABLE;
	}
	if ( clientNum < 0 || clientNum >= level.maxclients ) {
		return BOTLITE_STAMINA_COMFORTABLE;
	}

	percent = BotLite_StaminaPercent( clientNum );
	profile = g_botlite[clientNum].profile ? g_botlite[clientNum].profile
		: BotLite_GetProfile( g_botlite[clientNum].skill );

	if ( percent >= profile->staminaComfortablePct ) {
		return BOTLITE_STAMINA_COMFORTABLE;
	}
	if ( percent >= profile->staminaTightPct ) {
		return BOTLITE_STAMINA_TIGHT;
	}
	if ( percent >= profile->staminaCriticalPct ) {
		return BOTLITE_STAMINA_CRITICAL;
	}
	return BOTLITE_STAMINA_DRAINED;
}

const char *BotLite_StaminaBudgetName( botlite_stamina_t budget ) {
	switch ( budget ) {
	case BOTLITE_STAMINA_COMFORTABLE: return "COMFORTABLE";
	case BOTLITE_STAMINA_TIGHT: return "TIGHT";
	case BOTLITE_STAMINA_CRITICAL: return "CRITICAL";
	case BOTLITE_STAMINA_DRAINED: return "DRAINED";
	default: return "?";
	}
}

/*
 * Politica de gasto. La idea es que el bot no se quede sin stamina por goteo:
 * a medida que el presupuesto baja, primero se cortan las acciones caras.
 *
 *   COMFORTABLE -> todo
 *   TIGHT       -> se corta lo caro (zanzoken, transformacion)
 *   CRITICAL    -> solo lo barato (boost puntual)
 *   DRAINED     -> nada: toca desengancharse y recuperar
 */
/*
 * Fase 6.6 -- "estado critico": el unico momento en que escapar esta justificado.
 *
 * Reportado en juego: "el bot tiende a evadirme mucho en melee en lugar de
 * pelear. Lo de evadir y sanzoken solo debe hacerlo si el bot se encuentra en un
 * estado critico o con poderes a distancia".
 *
 * Es critico si la vida cayo por debajo del umbral configurado, o si la fatiga ya
 * no alcanza para sostener el duelo. Las amenazas a distancia son el otro caso
 * habilitado, y las resuelve la Fase 5 por su cuenta.
 */
qboolean BotLite_InCriticalState( gentity_t *bot, int clientNum ) {
	const botlite_profile_t *profile;
	botlite_info_t *info;
	int healthPct;
	botlite_stamina_t budget;

	if ( !bot || !bot->client || clientNum < 0 || clientNum >= BOTLITE_MAX_BOTS ) {
		return qfalse;
	}

	info = &g_botlite[clientNum];
	profile = info->profile ? info->profile : BotLite_GetProfile( info->skill );

	healthPct = BotLite_HealthPercent( clientNum );
	if ( profile && profile->escapeCriticalHealthPct > 0 &&
		 healthPct <= profile->escapeCriticalHealthPct ) {
		return qtrue;
	}

	budget = BotLite_StaminaBudget( clientNum );
	if ( budget == BOTLITE_STAMINA_CRITICAL || budget == BOTLITE_STAMINA_DRAINED ) {
		return qtrue;
	}
	return qfalse;
}

qboolean BotLite_StaminaAllowsSpend( int clientNum, botlite_spend_t cost ) {

	switch ( BotLite_StaminaBudget( clientNum ) ) {
	case BOTLITE_STAMINA_COMFORTABLE:
		return qtrue;
	case BOTLITE_STAMINA_TIGHT:
		return ( cost <= BOTLITE_SPEND_NORMAL ) ? qtrue : qfalse;
	case BOTLITE_STAMINA_CRITICAL:
		return ( cost <= BOTLITE_SPEND_CHEAP ) ? qtrue : qfalse;
	case BOTLITE_STAMINA_DRAINED:
	default:
		return qfalse;
	}
}

/* Sincroniza el bit que bg_pmove.c usa para decidir si el bot gasta stamina.
 * El cvar es del lado game y bg_pmove.c se compila tambien en el cgame, por eso
 * el puente es un bit de playerState en vez de una lectura directa del cvar. */
void BotLite_SyncStaminaMode( gentity_t *bot ) {
	if ( !bot || !bot->client ) {
		return;
	}

	if ( BotLite_RealStaminaEnabled() ) {
		bot->client->ps.options |= PSO_BOT_STAMINA;
	} else {
		bot->client->ps.options &= ~PSO_BOT_STAMINA;
	}
}

/*
 * ============================================================================
 * T2.5 -- Zanzoken defensivo (2da correccion tras partida real, 21-09 noche)
 * ============================================================================
 *
 * HISTORIAL DE DOS ERRORES, porque el segundo solo se entiende con el primero:
 *
 * 1er diseño (incorrecto): reaccionar a targetMeleeState en carga y
 *    teletransportarse. IMPOSIBLE: usingZanzoken solo se escribe dentro de
 *    PM_CheckZanzoken (bg_pmove.c:344), que se niega por completo mientras
 *    usingMelee este puesto (:316) -- y usingMelee queda fijo durante TODO el
 *    intercambio desde el primer contacto (PM_SyncMelee, :2532). El boton se
 *    presionaba y el motor lo descartaba en silencio.
 *
 * 2do diseño (tambien incorrecto, y mas sutil): al descubrir eso, mové la
 *    llamada al unico lugar donde !botInMelee estaba garantizado -- la etapa
 *    DISENGAGE de T2.8. Pero no verifiqué que ese lugar fuera ALCANZABLE:
 *    DISENGAGE solo corre dentro del modo RECOVER, que exige stamina <= 28%.
 *    En partida real la fatiga del bot nunca bajo de 35%, asi que el modo
 *    RECOVER jamas se activo y esta funcion quedo como codigo muerto: 0
 *    ejecuciones en toda la sesion. Acoplé una capacidad a un estado que casi
 *    nunca ocurre.
 *
 * 3er diseño (este): el zanzoken defensivo se evalua de forma INDEPENDIENTE,
 * desde el think loop, en cualquier momento en que el bot este fuera de melee
 * y realmente amenazado. Sigue respetando la restriccion dura del motor
 * (!botInMelee), pero ya no depende de que se cumpla un umbral de stamina.
 *
 * Que cuenta como 'amenazado' fuera de melee -- cualquiera de:
 *   - el bot viene de un knockback y el rival se le esta acercando
 *   - el rival esta cargando un ataque y el bot esta en su rango corto
 *   - el bot esta en desventaja de vida y el rival esta encima
 * Un simple 'el rival esta cerca' NO alcanza: durante una aproximacion normal
 * el bot QUIERE cerrar distancia, y teletransportarse seria contraproducente.
 */

/*
 * Fase 6.6 -- reescrito. Antes bastaba con "desventaja de vida y rival cerca",
 * que en la practica es casi todo el tiempo que el bot va perdiendo: cada vez que
 * el jugador cerraba a melee, el bot se teletransportaba. En el log se ven 14
 * escapes, todos con kb=0 y targetCharging=0, o sea todos disparados por esa
 * condicion, y el bot no registro ni un solo frame en un estado real de duelo
 * (nada de USING_SPEED, USING_POWER ni USING_BLOCK).
 *
 * Tambien se quito el disparador por carga de melee del rival: a un Power o Stun
 * cargado se le responde con Block o con Charge Breaker, que el bot ya tiene
 * (T2.2). Teletransportarse era renunciar al intercambio.
 *
 * Queda: estado critico de verdad, mas una amenaza inmediata. Los ataques a
 * distancia son el otro caso habilitado y los cubre la Fase 5 por separado.
 */
static qboolean BotLite_ZanzokenThreatPresent( gentity_t *bot, int clientNum, const botlite_snapshot_t *snapshot ) {
	if ( !BotLite_InCriticalState( bot, clientNum ) ) {
		return qfalse;
	}
	/* Recuperandose de un knockback con el rival encima: el peor momento para
		 * quedarse quieto, porque el siguiente golpe llega antes de poder responder. */
	if ( snapshot->botKnockbackTime > 0 ) {
		return qtrue;
	}
	/* Ya en critico y con el rival encima: romper el contacto es lo unico que
		 * queda por hacer. */
	if ( snapshot->hasTarget ) {
		return qtrue;
	}
	return qfalse;
}

qboolean BotLite_RunDefensiveZanzoken( gentity_t *bot, int clientNum, const botlite_snapshot_t *snapshot ) {
	botlite_info_t *info;
	const botlite_combat_policy_t *policy;
	const botlite_profile_t *profile;

	if ( !bot || !bot->client || !snapshot ) {
		return qfalse;
	}
	/* Restriccion dura del motor: con usingMelee puesto, PM_CheckZanzoken ni
		 * siquiera evalua el boton (bg_pmove.c:316). */
	if ( snapshot->botInMelee ) {
		return qfalse;
	}

	info = &g_botlite[clientNum];
	policy = BotLite_GetCombatPolicy( info->skill );
	profile = info->profile ? info->profile : BotLite_GetProfile( info->skill );

	if ( !policy || !policy->allowsDefensiveZanzoken ) {
		return qfalse;
	}
	if ( !snapshot->hasTarget || snapshot->dist >= profile->zanzokenEscapeDist ) {
		return qfalse;
	}
	if ( level.time < info->melee.zanzokenEscapeNextTime ) {
		return qfalse;
	}
	if ( !BotLite_ZanzokenThreatPresent( bot, clientNum, snapshot ) ) {
		return qfalse;
	}

	if ( !BotLite_StaminaAllowsSpend( clientNum, BOTLITE_SPEND_NORMAL ) ) {
		return qfalse;
	}
	if ( !( snapshot->botActionFlags & BOTACT_CAN_ZANZOKEN ) ) {
		return qfalse;
	}

	info->melee.zanzokenEscapeNextTime = level.time + 2200;
	BotLite_DebugLog( bot, va( "Defensive zanzoken: dist=%d kb=%d targetCharging=%d",
		(int)snapshot->dist,
		snapshot->botKnockbackTime,
		snapshot->targetCharging ? 1 : 0 ) );

	/*
	 * Escapar es alejarse, y el zanzoken escala la velocidad que el bot YA lleva
	 * (bg_pmove.c:336-340): sin direccion no se mueve de donde esta. Se gira en
	 * sentido contrario al rival y se avanza, en vez de retroceder de frente:
	 * forwardmove < 0 con el lock puesto pone al bot en stMeleeDegressing, que es
	 * justo el estado del que se viene escapando.
	 */
	if ( snapshot->hasTarget && snapshot->target && snapshot->target->client ) {
		vec3_t away;
		vec3_t angles;

		VectorSubtract( bot->client->ps.origin, snapshot->target->client->ps.origin, away );
		VectorClear( angles );
		angles[YAW] = ( away[0] == 0.0f && away[1] == 0.0f )
			? bot->client->ps.viewangles[YAW] : vectoyaw( away );
		BotLite_ApplyViewAngles( bot, angles );
	}
	BotLite_StartZanzoken( bot, clientNum, 127, 0, 0 );
	return qtrue;
}

/*
 * ============================================================================
 * T2.4 -- Carga de ki en las pausas
 * ============================================================================
 *
 * POWERLEVEL + rightmove>0 sube plCurrent (bg_pmove.c:709). Hasta ahora el bot solo
 * lo usaba como parche de curacion en skill 3.
 *
 * Dos costos que hacen que esto NO sea gratis, y que definen cuando conviene:
 *   - usingAlter anula la recuperacion de fatiga mientras dure (bg_pmove.c:647)
 *   - activa EF_AURA, que en el radar del jugador se marca como RADAR_BURST
 *     (g_radar.c:48): cargar ki es exactamente lo que delata la posicion
 *
 * Por eso solo se hace en pausas reales, y se corta apenas el rival se acerca.
 */

/* T4.1: distancia minima y objetivo de ki -- ahora profile->kiChargeMinSafeDist
 * y profile->kiChargeTargetPct. */

qboolean BotLite_RunKiCharge( gentity_t *bot, int clientNum, const botlite_snapshot_t *snapshot ) {
	botlite_info_t *info;
	const botlite_combat_policy_t *policy;
	const botlite_profile_t *profile;

	if ( !bot || !bot->client || !snapshot ) {
		return qfalse;
	}

	info = &g_botlite[clientNum];
	policy = BotLite_GetCombatPolicy( info->skill );
	profile = info->profile ? info->profile : BotLite_GetProfile( info->skill );

	if ( !policy || !policy->allowsKiCharge ) {
		info->recovery.kiChargeActive = qfalse;
		return qfalse;
	}
	/* ARBITRAJE ki vs stamina. Los dos recursos compiten de forma directa:
		 *   - statScale = 1 - (ki/kiMax): con el ki alto la recuperacion de fatiga
		 *     queda fijada en su minimo (0.25), o sea 3x mas lenta
		 *   - usingAlter anula el recovery por completo mientras se carga
		 * Cargar ki con la stamina baja es autodestructivo: la deja sin recuperarse y
		 * ademas la frena a futuro. Solo se carga con el presupuesto holgado. */
	if ( BotLite_StaminaBudget( clientNum ) != BOTLITE_STAMINA_COMFORTABLE ) {
		info->recovery.kiChargeActive = qfalse;
		return qfalse;
	}
	if ( BotLite_KiPercent( clientNum ) >= profile->kiChargeTargetPct ) {
		info->recovery.kiChargeActive = qfalse;
		return qfalse;
	}
	if ( snapshot->botInMelee || snapshot->botDisabled || snapshot->botFrozen ) {
		info->recovery.kiChargeActive = qfalse;
		return qfalse;
	}

	/* Condicion de aborto: es lo critico de esta tarea. Un bot cargando ki con el
	 * rival encima es dano gratis. Se exige distancia real, o bien que el rival
	 * este ocupado recuperandose. */
	if ( snapshot->hasTarget ) {
		if ( snapshot->targetCrashPartial || snapshot->targetStillRecovering ) {
			/* Ventana segura: el rival no puede castigar. */
		} else if ( !snapshot->hasLineOfSight ) {
			/* Sin linea de vista tampoco puede. */
		} else if ( snapshot->dist < profile->kiChargeMinSafeDist ) {
			info->recovery.kiChargeActive = qfalse;
			return qfalse;
		}
		if ( BotLite_TargetIsChargingMelee( snapshot ) || snapshot->targetCharging ) {
			info->recovery.kiChargeActive = qfalse;
			return qfalse;
		}
	}

	/* Se llama cada frame: loguear solo el inicio de la ventana. */
	if ( !info->recovery.kiChargeActive ) {
		info->recovery.kiChargeActive = qtrue;
		BotLite_DebugLog( bot, va( "Charging ki %d%% dist=%d",
			BotLite_KiPercent( clientNum ), (int)snapshot->dist ) );
	}
	/* forwardmove debe quedar en 0: con forwardmove>0 esto seria subir de tier. */
	BotLite_EA_MoveForward( bot, 0 );
	BotLite_EA_Button( bot, BUTTON_POWERLEVEL );
	BotLite_EA_MoveRight( bot, 127 );
	return qtrue;
}

/*
 * ============================================================================
 * T2.7 -- Decision de desenganche: pelear / defender / recuperarse
 * ============================================================================
 *
 * Hasta aca el bot no tenia el concepto de retirarse: peleaba hasta morir. Este
 * es el disparador del modo RECOVER.
 *
 * La histeresis no es un detalle de tuning: sin umbrales distintos para entrar y
 * salir, el bot oscila entre huir y pelear en cada frame que cruza la linea.
 */

/* Entra a recuperarse por debajo de esto; sale recien por encima del otro. */
/* T4.1: umbrales de entrada/salida -- ahora profile->recoverEnterPct/ExitPct. */
/* Techo duro: un bot que se retira indefinidamente no es dificil, es tedioso. */
/* T4.1: techo de tiempo -- ahora profile->recoverMaxMs. */

static qboolean BotLite_RecoverHasAdvantage( const botlite_snapshot_t *snapshot ) {
	if ( !snapshot || !snapshot->hasTarget ) {
		return qfalse;
	}
	/* Rematar a un rival que ya no puede responder vale mas que conservar stamina. */
	if ( snapshot->targetCrashPartial || snapshot->targetKnockbackTime > 0 ) {
		return qtrue;
	}
	if ( snapshot->targetHealth > 0 && snapshot->botHealth > snapshot->targetHealth * 2 ) {
		return qtrue;
	}
	return qfalse;
}

qboolean BotLite_UpdateRecoverMode( gentity_t *bot, int clientNum, const botlite_snapshot_t *snapshot ) {
	botlite_info_t *info;
	const botlite_combat_policy_t *policy;
	const botlite_profile_t *profile;
	int staminaPct;
	int healthPct;

	if ( !bot || !bot->client || !snapshot ) {
		return qfalse;
	}

	info = &g_botlite[clientNum];
	policy = BotLite_GetCombatPolicy( info->skill );
	profile = info->profile ? info->profile : BotLite_GetProfile( info->skill );

	if ( !policy || !policy->allowsRecoverMode ) {
		info->recovery.recoverActive = qfalse;
		return qfalse;
	}

	staminaPct = BotLite_StaminaPercent( clientNum );
	healthPct = BotLite_HealthPercent( clientNum );

	if ( info->recovery.recoverActive ) {
		qboolean exitToSearch = qfalse;
		/* Salidas: recursos recuperados, se acabo el tiempo, o apareció una
		 * ventana que no conviene desperdiciar. Las dos primeras salen a buscar
		 * el jugador mas cercano (T3.6): no hay razon para volver puntualmente al
		 * mismo rival si ya no esta cerca. La tercera SI vuelve al mismo target,
		 * porque la ventana es especifica de el (crasheado/en desventaja de vida).
		 */
		if ( staminaPct >= profile->recoverExitPct && healthPct >= profile->recoverHealthExitPct ) {
			info->recovery.recoverActive = qfalse;
			exitToSearch = qtrue;
			BotLite_DebugLog( bot, va( "Recover done -> seeking nearest player stamina=%d%%", staminaPct ) );
		} else if ( level.time >= info->recovery.recoverEndTime ) {
			info->recovery.recoverActive = qfalse;
			info->recovery.recoverNextAllowedTime = level.time + profile->recoverCooldownMs;
			exitToSearch = qtrue;
			BotLite_DebugLog( bot, va( "Recover timeout -> seeking nearest player stamina=%d%%", staminaPct ) );
		} else if ( BotLite_RecoverHasAdvantage( snapshot ) ) {
			info->recovery.recoverActive = qfalse;
			BotLite_DebugLog( bot, "Recover cut short: window on target" );
		}
		if ( !info->recovery.recoverActive && info->runtime.mode == BOTLITE_MODE_RECOVER ) {
			info->runtime.mode = exitToSearch ? BOTLITE_MODE_SEARCH : BOTLITE_MODE_COMBAT;
			if ( exitToSearch ) {
				info->runtime.lastTargetNum = -1;
			}
		}
		return info->recovery.recoverActive;
	}

	/*
	 * Enfriamiento entre episodios. Sin esto, una salida por timeout con la vida
	 * todavia baja reentraba al frame siguiente y el bot se pasaba la partida
	 * entrando y saliendo de RECOVER sin pelear (visto en el log del 22-09: tres
	 * "Recover timeout" seguidos con health=28%).
	 */
	if ( level.time < info->recovery.recoverNextAllowedTime ) {
		return qfalse;
	}

	/* Dos disparadores independientes. El de vida faltaba: el bot recibia dano
		 * hasta morir sin retirarse nunca, porque su stamina se mantenia sana. */
	if ( staminaPct > profile->recoverEnterPct &&
		 healthPct > profile->recoverHealthEnterPct ) {
		return qfalse;
	}
	/* Con ventaja clara se sigue peleando aunque duela. */
	if ( BotLite_RecoverHasAdvantage( snapshot ) ) {
		return qfalse;
	}

	info->recovery.recoverActive = qtrue;
	info->recovery.recoverEndTime = level.time + profile->recoverMaxMs;
	info->recovery.recoverStage = BOTLITE_RECOVER_STAGE_DISENGAGE;
	info->recovery.recoverStageTime = level.time;
	info->recovery.recoverLastHealth = bot->client->ps.powerLevel[plHealth];
	info->recovery.recoverHealthProgressTime = level.time;
	info->runtime.mode = BOTLITE_MODE_RECOVER;
	BotLite_DebugLog( bot, va( "Recover start stamina=%d%% health=%d%% ki=%d%%",
		staminaPct, healthPct, BotLite_KiPercent( clientNum ) ) );
	return qtrue;
}

/*
 * ============================================================================
 * T2.8 -- Ciclo de recuperacion: huir -> bajar ki de una -> descansar
 * ============================================================================
 *
 * Tres etapas secuenciales, no simultaneas. Corregido tras probarlo en partida
 * real (21-09): la primera version bajaba ki en pleno combate, ademas de hacerlo
 * a pulsos cortos que lo volvian innecesariamente lento.
 *
 * 1) DISENGAGE -- huir hasta romper el contacto (fuera de melee y a distancia
 *    segura, o sin linea de vista). Mientras el rival este encima, NO se toca
 *    el ki: bajarlo no sirve de nada si de todos modos van a seguir pegandole.
 *
 * 2) DRAIN -- ya lejos, bajar el ki de UN SOLO TIRON hasta el piso, sosteniendo
 *    el boton en vez de pulsarlo. La bajada de ki (bg_pmove.c:709-707, rama
 *    rightmove<0) corre CADA TICK DE FISICA, fuera del bloque de 100ms que rige
 *    la regeneracion -- no hay ninguna razon para pulsarla, sostenerla es mas
 *    rapido y el costo (usingAlter bloqueando el recovery de fatiga) se paga
 *    una sola vez en vez de repetirse en cada pulso.
 *
 * 3) REST -- soltar el boton por completo y quedarse quieto. Recien aca importa
 *    la formula de bg_pmove.c:636-646: con el ki en el piso, statScale queda en
 *    su techo (0.75, hasta 3x mas rapido que con ki alto) y quieto de pie
 *    idleScale da otro x2.8. Es el unico momento en que la fatiga sube.
 *
 * Si el rival vuelve a cerrar distancia en cualquier etapa, se vuelve a
 * DISENGAGE: recuperarse en pleno combate no es la idea.
 */

/* Distancia (o LOS rota) a partir de la cual se considera 'ya lejos'. */
/* T4.1: distancia segura -- ahora profile->recoverSafeDist. Piso de ki --
 * ahora profile->recoverKiFloorPct. */
/* T3.6 -- skill 3 baja mas: alfa del blip en el radar del rival es
 * literalmente plCurrent/plMaximum (cg_radar.c), asi que 22%% sigue siendo
 * bastante visible, solo tenue. Zanzoken no depende de ki (solo de fatiga,
 * bg_pmove.c:296), asi que bajar mas no lo bloquea, solo lo hace mas lento
 * -- aceptable para un bot que en ese momento esta intentando NO pelear. */
/* T4.1: piso de ki oculto (skill3) -- ahora profile->recoverKiFloorHiddenPct. */

static qboolean BotLite_RecoverIsDisengaged( const botlite_snapshot_t *snapshot, float safeDist ) {
	if ( !snapshot ) {
		return qtrue;
	}
	if ( snapshot->botInMelee ) {
		return qfalse;
	}
	if ( !snapshot->hasTarget ) {
		return qtrue;
	}
	if ( !snapshot->hasLineOfSight ) {
		return qtrue;
	}
	return ( snapshot->dist > safeDist ) ? qtrue : qfalse;
}

static void BotLite_RecoverEnterStage( botlite_info_t *info, int stage ) {
	info->recovery.recoverStage = stage;
	info->recovery.recoverStageTime = level.time;
}

static void BotLite_RecoverRunFlee( gentity_t *bot, const botlite_snapshot_t *snapshot ) {
	vec3_t delta;
	vec3_t angles;
	float yaw;

	if ( snapshot && snapshot->hasTarget && snapshot->target && snapshot->target->client ) {
		VectorSubtract( bot->client->ps.origin, snapshot->target->client->ps.origin, delta );
		yaw = ( delta[0] == 0.0f && delta[1] == 0.0f ) ? bot->client->ps.viewangles[YAW] : vectoyaw( delta );
	} else {
		yaw = bot->client->ps.viewangles[YAW];
	}
	VectorClear( angles );
	angles[YAW] = yaw;
	BotLite_ApplyViewAngles( bot, angles );
	BotLite_EA_MoveForward( bot, 127 );
}

qboolean BotLite_RunRecoverCycle( gentity_t *bot, int clientNum, const botlite_snapshot_t *snapshot ) {
	botlite_info_t *info;
	const botlite_profile_t *profile;

	if ( !bot || !bot->client ) {
		return qfalse;
	}

	info = &g_botlite[clientNum];
	if ( !info->recovery.recoverActive ) {
		return qfalse;
	}
	profile = info->profile ? info->profile : BotLite_GetProfile( info->skill );

	/* Mirar al rival aunque se este recuperando: perderlo de vista es peor
		 * (salvo mientras huye, que ahi mirar hacia atras lo frenaria). */
	if ( info->recovery.recoverStage != BOTLITE_RECOVER_STAGE_DISENGAGE &&
		 snapshot && snapshot->hasTarget && snapshot->target && snapshot->target->client ) {
		BotLite_FaceTarget( bot, snapshot->target );
	}

	if ( !BotLite_RecoverIsDisengaged( snapshot, profile->recoverSafeDist ) ) {
		if ( info->recovery.recoverStage != BOTLITE_RECOVER_STAGE_DISENGAGE ) {
			BotLite_DebugLog( bot, "Recover: target closed in, disengaging again" );
			BotLite_RecoverEnterStage( info, BOTLITE_RECOVER_STAGE_DISENGAGE );
		}
		/* El zanzoken defensivo ya no se evalua desde aca: se movio al think loop
			 * para que no dependa de que el modo RECOVER llegue a activarse. */
		BotLite_RecoverRunFlee( bot, snapshot );
		return qtrue;
	}

	if ( info->recovery.recoverStage == BOTLITE_RECOVER_STAGE_DISENGAGE ) {
		BotLite_RecoverEnterStage( info, BOTLITE_RECOVER_STAGE_DRAIN );
		BotLite_DebugLog( bot, "Recover: disengaged, draining ki" );
	}

	if ( info->recovery.recoverStage == BOTLITE_RECOVER_STAGE_DRAIN ) {
		int kiFloor = ( info->skill == 3 ) ? profile->recoverKiFloorHiddenPct : profile->recoverKiFloorPct;
		if ( BotLite_KiPercent( clientNum ) > kiFloor ) {
			/* Sostenido, no pulsado: la bajada de ki no esta gateada por el
				 * bloque de 100ms de bg_pmove.c, asi que pulsarla solo la hacia mas
				 * lenta sin ganar nada a cambio. */
			BotLite_EA_MoveForward( bot, 0 );
			BotLite_EA_Button( bot, BUTTON_POWERLEVEL );
			BotLite_EA_MoveRight( bot, -127 );
			return qtrue;
		}
		BotLite_DebugLog( bot, "Recover: ki at floor, resting" );
		BotLite_RecoverEnterStage( info, BOTLITE_RECOVER_STAGE_REST );
	}

	/*
	 * REST: sin botones, quieto. Es el momento en que sube la fatiga (statScale
	 * en su techo con el ki en el piso, mas el x2.8 de idleScale por estar
	 * parado). Se sale cuando la fatiga llego al objetivo.
	 */
	if ( info->recovery.recoverStage == BOTLITE_RECOVER_STAGE_REST ) {
		if ( BotLite_StaminaPercent( clientNum ) < profile->recoverRestExitPct ) {
			return qtrue;
		}
		BotLite_DebugLog( bot, va( "Recover: stamina %d%%, recargando ki",
			BotLite_StaminaPercent( clientNum ) ) );
		BotLite_RecoverEnterStage( info, BOTLITE_RECOVER_STAGE_RECHARGE );
		info->recovery.recoverLastHealth = bot->client->ps.powerLevel[plHealth];
		info->recovery.recoverHealthProgressTime = level.time;
	}

	/*
	 * RECHARGE -- la mitad que faltaba del ciclo.
	 *
	 * El ciclo original terminaba en REST y ahi se quedaba: el bot bajaba el ki
	 * al piso, recuperaba la fatiga y NUNCA volvia a cargar. Reportado en juego:
	 * "deja de pelear y no vuelve a cargar ki en ningun momento, aun cuando la
	 * stamina ya se recupero al 100".
	 *
	 * Peor: la vida tampoco volvia. plHealth solo sube cuando plCurrent ya llego
	 * a plMaximum y se sigue cargando (bg_pmove.c:737-746, rama isBreakingLimit),
	 * convirtiendo plHealthPool a razon de raise*0.3. Con el ki clavado en el
	 * piso eso no pasa nunca, asi que la condicion de salida por vida era
	 * inalcanzable y el modo salia solo por timeout... para reentrar al instante
	 * porque la vida seguia baja. Ese era el bucle que dejaba al bot sin pelear.
	 *
	 * El balance que pedia la especificacion ("cargar para sanarse sin generar
	 * mas fatiga de la deseada") sale de una asimetria real del motor: cargar
	 * ki por DEBAJO del 100%% no cuesta fatiga -- los plUseFatigue estan todos
	 * dentro de la rama de plCurrent == plMaximum. Lo que si hace todo el tramo
	 * es frenar la recuperacion de fatiga, porque usingAlter la anula
	 * (bg_pmove.c:647). O sea: subir a 100%% es barato, empujar mas alla para
	 * curarse es lo que cuesta. Por eso el piso de stamina solo corta el empuje.
	 */
	if ( info->recovery.recoverStage == BOTLITE_RECOVER_STAGE_RECHARGE ) {
		int kiPct;
		int staminaPct;
		int healthNow;

		kiPct = BotLite_KiPercent( clientNum );
		staminaPct = BotLite_StaminaPercent( clientNum );
		healthNow = bot->client->ps.powerLevel[plHealth];

		if ( healthNow > info->recovery.recoverLastHealth ) {
			info->recovery.recoverLastHealth = healthNow;
			info->recovery.recoverHealthProgressTime = level.time;
		}

		/* Gastar la fatiga que se acaba de recuperar seria dar vueltas en
			 * circulo. Vuelve a descansar en vez de insistir. */
		if ( staminaPct < profile->recoverRechargeMinStaminaPct ) {
			BotLite_DebugLog( bot, va( "Recover: stamina %d%% muy baja para recargar, descansando",
				staminaPct ) );
			BotLite_RecoverEnterStage( info, BOTLITE_RECOVER_STAGE_REST );
			return qtrue;
		}

		/* Objetivo de ki alcanzado y por debajo del tope: no hay curacion que
			 * buscar (curar exige llegar a plMaximum y seguir), asi que ya esta. */
		if ( profile->recoverRechargeKiPct < 100 && kiPct >= profile->recoverRechargeKiPct ) {
			BotLite_DebugLog( bot, va( "Recover: ki al %d%%, volviendo al combate", kiPct ) );
			info->recovery.recoverActive = qfalse;
			info->recovery.recoverNextAllowedTime = level.time + profile->recoverCooldownMs;
			info->runtime.mode = BOTLITE_MODE_SEARCH;
			info->runtime.lastTargetNum = -1;
			return qtrue;
		}

		/* Con el ki ya arriba y la vida sin moverse, no hay pool que convertir:
			 * seguir cargando no cura, solo expone (EF_AURA marca RADAR_BURST). */
		if ( kiPct >= 100 &&
			 level.time - info->recovery.recoverHealthProgressTime > BOTLITE_RECOVER_HEAL_STALL_MS ) {
			BotLite_DebugLog( bot, "Recover: sin pool para curar, volviendo al combate" );
			info->recovery.recoverActive = qfalse;
			info->recovery.recoverNextAllowedTime = level.time + profile->recoverCooldownMs;
			info->runtime.mode = BOTLITE_MODE_SEARCH;
			info->runtime.lastTargetNum = -1;
			return qtrue;
		}

		BotLite_EA_MoveForward( bot, 0 );
		BotLite_EA_Button( bot, BUTTON_POWERLEVEL );
		BotLite_EA_MoveRight( bot, 127 );
		return qtrue;
	}

	return qtrue;
}

/*
 * ============================================================================
 * T3.4 -- Break-limit ofensivo
 * ============================================================================
 *
 * PM_UsePowerLevel (bg_pmove.c:721-744): seguir cargando ki despues de llegar al
 * 100% entra en isBreakingLimit. Ahi el ki extra se convierte en healthPool y
 * maximumPool en vez de ki normal -- o sea que sostenerlo un poco mas alla del
 * tope hace crecer vida y ki maximo, de forma casi permanente para el resto del
 * combate. El costo: plUseFatigue += raise*0.25 y el recovery de fatiga queda
 * en cero mientras dura (bg_pmove.c:647).
 *
 * A diferencia del hack de "curacion" de skill 3 (BotLite_RunSkill3RecoveryHeal,
 * que solo se dispara con el rival MUERTO), esto es una decision TACTICA en
 * pleno combate: cuando el rival esta vulnerable (crasheado, en knockback,
 * recuperandose) y por lo tanto no puede castigar la exposicion, conviene
 * convertir ese margen de seguridad en un recurso permanente en vez de
 * desperdiciarlo. Ventana corta y con techo de tiempo: no es gratis.
 */

/* Ki minimo antes de considerar empujar el limite -- si esta bajo, primero hay
 * que cargar lo normal (T2.4), no tiene sentido entrar directo en break-limit. */
/* T4.1: umbral de ki y ventana -- ahora profile->breakLimitMinKiPct/HoldMs. */

static qboolean BotLite_TargetCannotPunish( const botlite_snapshot_t *snapshot ) {
	if ( !snapshot || !snapshot->hasTarget ) {
		return qfalse;
	}
	return ( snapshot->targetCrashPartial || snapshot->targetKnockbackTime > 0 ||
		snapshot->targetStillRecovering ) ? qtrue : qfalse;
}

qboolean BotLite_RunOffensiveBreakLimit( gentity_t *bot, int clientNum, const botlite_snapshot_t *snapshot ) {
	botlite_info_t *info;
	const botlite_combat_policy_t *policy;
	const botlite_profile_t *profile;

	if ( !bot || !bot->client || !snapshot ) {
		return qfalse;
	}

	info = &g_botlite[clientNum];

	if ( !BotLite_TargetCannotPunish( snapshot ) ) {
		info->recovery.breakLimitDecided = qfalse;
		info->recovery.breakLimitUntil = 0;
		return qfalse;
	}

	/* Ya en curso: sostener hasta el limite de tiempo. */
	if ( info->recovery.breakLimitUntil > level.time ) {
		BotLite_EA_MoveForward( bot, 0 );
		BotLite_EA_Button( bot, BUTTON_POWERLEVEL );
		BotLite_EA_MoveRight( bot, 127 );
		return qtrue;
	}
	if ( info->recovery.breakLimitDecided ) {
		/* Ya se hizo (o se decidio no hacerlo) para esta ventana de vulnerabilidad. */
		return qfalse;
	}

	info->recovery.breakLimitDecided = qtrue;

	policy = BotLite_GetCombatPolicy( info->skill );
	profile = info->profile ? info->profile : BotLite_GetProfile( info->skill );
	if ( !policy || !policy->allowsOffensiveBreakLimit ) {
		return qfalse;
	}
	if ( snapshot->botInMelee || snapshot->botDisabled || snapshot->botFrozen ) {
		return qfalse;
	}
	if ( BotLite_KiPercent( clientNum ) < profile->breakLimitMinKiPct ) {
		return qfalse;
	}
	/* Rompe su propia recuperacion de fatiga mientras dura: solo si ya viene bien. */
	if ( BotLite_StaminaBudget( clientNum ) != BOTLITE_STAMINA_COMFORTABLE ) {
		return qfalse;
	}

	info->recovery.breakLimitUntil = level.time + profile->breakLimitHoldMs;
	BotLite_DebugLog( bot, va( "Offensive break-limit: ki=%d%% window=%dms",
		BotLite_KiPercent( clientNum ), profile->breakLimitHoldMs ) );
	BotLite_EA_MoveForward( bot, 0 );
	BotLite_EA_Button( bot, BUTTON_POWERLEVEL );
	BotLite_EA_MoveRight( bot, 127 );
	return qtrue;
}
