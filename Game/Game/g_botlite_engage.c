#include "g_local.h"
#include "g_botlite.h"

/*
 * ============================================================================
 * FASE 6 -- Alternar melee / ranged por DECISION, no por distancia
 * ============================================================================
 *
 * Como estaba antes: BotLite_UpdateHybridRangeMode encendia forceMelee cuando
 * dist <= ranged_to_melee_distance y lo apagaba cuando dist > melee_to_ranged_
 * distance. Con los valores de los .cfg (skill2: 10000/10000, skill3: 10000/
 * 12000) y las distancias reales de combate medidas en el log (mediana ~2019),
 * el bot estaba SIEMPRE por debajo del umbral: en la practica era un bot de
 * melee puro que solo disparaba si el rival se iba muy lejos. La distancia no
 * era una entrada de la decision, era la decision entera.
 *
 * Ahora hay una capa de intencion: el bot puntua melee contra ranged con varios
 * terminos y la distancia es UNO de ellos (en skill 3, de los que menos pesa).
 * La intencion tiene permanencia minima y necesita un margen para invertirse,
 * asi no oscila cada frame.
 *
 * ---------------------------------------------------------------------------
 * De donde salen los terminos (verificado en bg_pmove.c, no estimado):
 *
 * KI ......... El dano de Power melee es plCurrent * 0.15 * stMeleeAttack
 *              (bg_pmove.c:2460) y el coste en fatiga de un ataque de ki escala
 *              con energyScale = plCurrent / plMaximum (bg_pmove.c:2769). O sea:
 *              con ki bajo el ranged pega poco y el Speed melee sigue entero,
 *              porque ese sale de la fatiga (damage = plFatigue * 0.013,
 *              bg_pmove.c:2583). Ki bajo -> melee.
 *
 * STAMINA .... Un Power melee cuesta plMaximum * 0.05 de fatiga y la rama del
 *              breaker cuesta 0.10 (bg_pmove.c:2452 y 2544). Sin fatiga el melee
 *              no se sostiene. Stamina alta -> melee.
 *
 * VIDA ....... El dano hecho se convierte en vida y maximo propios. Power melee
 *              convierte a 1.0 / 0.8 (bg_pmove.c:2479-2480), Speed melee a
 *              0.7 / 0.5 (bg_pmove.c:2594-2595) y un ataque de ki a 0.7 / 0.3
 *              (g_usermissile.c:549-550). El melee es el mejor curador del
 *              juego. Por eso vida baja empuja a melee -- pero solo si queda ki
 *              para alimentar el Power, porque sin ki ese golpe no hace dano y
 *              entonces tampoco cura.
 *
 * RIVAL ...... Entrar en melee le fuerza PM_WeaponRelease() al rival
 *              (bg_pmove.c:2742): meterse encima le corta una carga de ki. Al
 *              reves, bloquear reduce el Speed melee a 0.2x y el Power a 0.3x
 *              (bg_pmove.c:2588 y 2461), asi que contra un rival que bloquea el
 *              melee es tirar fatiga.
 *
 * DISTANCIA .. Termino suave, ya no una compuerta.
 *
 * EFECTO ..... Lo unico MEDIDO en vez de deducido: media movil del dano neto
 *              bajo cada intencion. Ver la advertencia de honestidad en
 *              BotLite_SampleEngageEffect sobre por que es una aproximacion.
 * ---------------------------------------------------------------------------
 */

/* Peso del termino nuevo en la media movil de efectividad. */
#define BOTLITE_ENGAGE_EMA_ALPHA		0.12f
/* Muestras separadas por mas que esto no son comparables (respawn, pausa). */
#define BOTLITE_ENGAGE_SAMPLE_MAX_MS	1000
/* Histeresis del kiting: una vez que empezo a abrir hueco, sigue hasta pasar el
 * umbral multiplicado por esto. Sin margen de salida oscilaba cada tick -- en el
 * log de la primera prueba se ven 4 cambios de tactica en 4 frames seguidos
 * (RANGED_PRESSURE <-> RANGED_KITE) rozando los 900 de un lado y del otro. */
#define BOTLITE_ENGAGE_KITE_EXIT_SCALE	1.35f
/* Cuanto sigue contando como "kiteando" despues del ultimo frame de retroceso. */
#define BOTLITE_ENGAGE_KITE_GRACE_MS	400
/* Ki minimo para que el Power melee haga dano suficiente como para curar. */
#define BOTLITE_ENGAGE_HEAL_MIN_KI_PCT	35

const char *BotLite_EngageIntentName( botlite_engage_intent_t intent ) {
	switch ( intent ) {
	case BOTLITE_ENGAGE_MELEE:	return "MELEE";
	case BOTLITE_ENGAGE_RANGED:	return "RANGED";
	default:					return "NONE";
	}
}

void BotLite_ResetEngageState( int clientNum, qboolean fullReset ) {
	botlite_info_t *info;

	if ( clientNum < 0 || clientNum >= BOTLITE_MAX_BOTS ) {
		return;
	}
	info = &g_botlite[clientNum];
	info->engage.intent = BOTLITE_ENGAGE_NONE;
	info->engage.nextSwitchTime = 0;
	info->engage.lastScore = 0.0f;
	info->engage.lastLogTime = 0;
	info->engage.kiteUntil = 0;
	info->engage.sampleTargetNum = -1;
	info->engage.sampleTargetHealth = 0;
	info->engage.sampleBotHealth = 0;
	info->engage.sampleTime = 0;
	if ( fullReset ) {
		/* La efectividad aprendida sobrevive a un cambio de objetivo: es una
			 * lectura del propio bot, no del rival. Solo se borra al respawnear. */
		info->engage.meleeEffect = 0.0f;
		info->engage.rangedEffect = 0.0f;
	}
}

qboolean BotLite_EngageIntentActive( int clientNum ) {
	botlite_info_t *info;
	const botlite_combat_policy_t *policy;

	if ( clientNum < 0 || clientNum >= BOTLITE_MAX_BOTS ) {
		return qfalse;
	}
	info = &g_botlite[clientNum];
	policy = BotLite_GetCombatPolicy( info->skill );
	if ( !policy || !policy->allowsEngageIntent ) {
		return qfalse;
	}
	/* Si el skill no tiene las dos presiones habilitadas no hay nada que alternar. */
	if ( !policy->allowsRangedPressure || !policy->allowsMeleePressure ) {
		return qfalse;
	}
	return qtrue;
}

botlite_engage_intent_t BotLite_GetEngageIntent( int clientNum ) {
	if ( clientNum < 0 || clientNum >= BOTLITE_MAX_BOTS ) {
		return BOTLITE_ENGAGE_NONE;
	}
	return g_botlite[clientNum].engage.intent;
}

static float BotLite_ClampUnit( float value ) {
	if ( value > 1.0f ) {
		return 1.0f;
	}
	if ( value < -1.0f ) {
		return -1.0f;
	}
	return value;
}

/*
 * Media movil del dano neto bajo la intencion vigente.
 *
 * ADVERTENCIA: esto compara la vida del rival entre dos muestras. En un FFA el
 * rival puede estar recibiendo dano de un tercero y el bot se lo anota como
 * propio. No hay una fuente limpia de "dano que hice yo" accesible desde aca sin
 * tocar g_combat.c. Se acota el problema exigiendo que siga siendo el mismo
 * objetivo y descartando muestras demasiado separadas, pero sigue siendo una
 * aproximacion. Por eso este termino pesa poco y en skill 1 y 2 arranca en 0.
 */
static void BotLite_SampleEngageEffect( int clientNum, const botlite_snapshot_t *snapshot ) {
	botlite_info_t *info;
	int elapsed;
	float dealt;
	float taken;
	float net;
	float scale;

	info = &g_botlite[clientNum];
	if ( !snapshot->hasTarget || !snapshot->target ) {
		info->engage.sampleTargetNum = -1;
		return;
	}

	if ( info->engage.sampleTargetNum != snapshot->target->s.number ) {
		info->engage.sampleTargetNum = snapshot->target->s.number;
		info->engage.sampleTargetHealth = snapshot->targetHealth;
		info->engage.sampleBotHealth = snapshot->botHealth;
		info->engage.sampleTime = level.time;
		return;
	}

	elapsed = level.time - info->engage.sampleTime;
	if ( elapsed <= 0 || elapsed > BOTLITE_ENGAGE_SAMPLE_MAX_MS ) {
		info->engage.sampleTargetHealth = snapshot->targetHealth;
		info->engage.sampleBotHealth = snapshot->botHealth;
		info->engage.sampleTime = level.time;
		return;
	}

	dealt = (float)( info->engage.sampleTargetHealth - snapshot->targetHealth );
	taken = (float)( info->engage.sampleBotHealth - snapshot->botHealth );
	/* Valores negativos = alguien se curo. No cuenta como dano al reves. */
	if ( dealt < 0.0f ) {
		dealt = 0.0f;
	}
	if ( taken < 0.0f ) {
		taken = 0.0f;
	}

	info->engage.sampleTargetHealth = snapshot->targetHealth;
	info->engage.sampleBotHealth = snapshot->botHealth;
	info->engage.sampleTime = level.time;

	scale = ( snapshot->botKiMax > 0 ) ? (float)snapshot->botKiMax : 1.0f;
	net = BotLite_ClampUnit( ( ( dealt - taken ) / scale ) * 100.0f );

	if ( info->engage.intent == BOTLITE_ENGAGE_MELEE ) {
		info->engage.meleeEffect += ( net - info->engage.meleeEffect ) * BOTLITE_ENGAGE_EMA_ALPHA;
	} else if ( info->engage.intent == BOTLITE_ENGAGE_RANGED ) {
		info->engage.rangedEffect += ( net - info->engage.rangedEffect ) * BOTLITE_ENGAGE_EMA_ALPHA;
	}
}

/* Estado del rival: suma de incentivos discretos, acotada a [-1,+1]. */
static float BotLite_EngageTargetStateTerm( const botlite_snapshot_t *snapshot ) {
	float term;

	term = 0.0f;
	/* Meterse encima de una carga de ki se la cancela (PM_WeaponRelease). */
	if ( snapshot->targetCharging ) {
		term += 0.90f;
	}
	/* Bloqueando: el melee pierde casi todo su dano contra el. */
	if ( snapshot->targetBlocking ) {
		term -= 0.80f;
	}
	/* En knockback esta volando: no hay a quien agarrar, pero si a quien tirarle. */
	if ( snapshot->targetKnockbackTime > 0 ) {
		term -= 0.60f;
	}
	/* Un tier por encima pega mas fuerte de cerca: conviene no darle el duelo. */
	if ( snapshot->targetTier > snapshot->botTier ) {
		term -= 0.50f;
	} else if ( snapshot->botTier > snapshot->targetTier ) {
		term += 0.35f;
	}
	return BotLite_ClampUnit( term );
}

static float BotLite_ComputeEngageScore( int clientNum, const botlite_snapshot_t *snapshot, const botlite_profile_t *profile ) {
	botlite_info_t *info;
	float total;
	float weightSum;
	float term;
	float w;
	int kiPct;
	int staminaPct;
	int healthPct;

	info = &g_botlite[clientNum];
	total = 0.0f;
	weightSum = 0.0f;

	kiPct = BotLite_KiPercent( clientNum );
	staminaPct = BotLite_RealStaminaEnabled() ? BotLite_StaminaPercent( clientNum ) : 100;
	healthPct = BotLite_HealthPercent( clientNum );

	/* Personalidad del archetype. */
	w = profile->engageWeightBias;
	if ( w > 0.0f ) {
		term = BotLite_ClampUnit( info->rushTendency - info->rangedBias );
		total += term * w;
		weightSum += w;
	}

	/*
	 * Ki bajo -> el ranged pega poco y cuesta ki; el Speed melee sale de fatiga.
	 *
	 * UNILATERAL (Fase 6.6). Antes este termino era simetrico y con el ki al 100%%
	 * aportaba -1.10 hacia ranged. Esa mitad no estaba justificada: tener mucho ki
	 * hace que el ranged sea ASEQUIBLE, que no es lo mismo que hacer que el melee
	 * sea mala idea. Sumado al termino de vida, un bot sano y cargado arrancaba con
	 * -2.00 sobre una suma de pesos de 6.40, o sea estructuralmente inclinado a
	 * pelear de lejos. En el log: 48 tacticas ranged contra 10 de melee, y ni un
	 * solo frame en un estado real de duelo.
	 */
	w = profile->engageWeightKi;
	if ( w > 0.0f ) {
		term = BotLite_ClampUnit( ( 50.0f - (float)kiPct ) / 50.0f );
		if ( term < 0.0f ) {
			term = 0.0f;
		}
		total += term * w;
		weightSum += w;
	}

	/* Stamina alta -> el melee se puede sostener. */
	w = profile->engageWeightStamina;
	if ( w > 0.0f ) {
		term = BotLite_ClampUnit( ( (float)staminaPct - 50.0f ) / 50.0f );
		total += term * w;
		weightSum += w;
	}

	/* Vida baja -> melee, que convierte dano en vida mejor que cualquier ataque
		 * de ki. Pero solo con ki suficiente para alimentar el Power melee. */
	/* Tambien unilateral, por la misma razon: estar sano no es un motivo para
		 * pelear de lejos. Si acaso es lo contrario, porque el melee es el mejor
		 * convertidor de dano en vida del juego. */
	w = profile->engageWeightHealth;
	if ( w > 0.0f ) {
		if ( kiPct >= BOTLITE_ENGAGE_HEAL_MIN_KI_PCT ) {
			term = BotLite_ClampUnit( ( 50.0f - (float)healthPct ) / 50.0f );
			if ( term < 0.0f ) {
				term = 0.0f;
			}
		} else {
			term = 0.0f;
		}
		total += term * w;
		weightSum += w;
	}

	/* Lo que el rival esta haciendo ahora mismo. */
	w = profile->engageWeightTargetState;
	if ( w > 0.0f ) {
		term = BotLite_EngageTargetStateTerm( snapshot );
		total += term * w;
		weightSum += w;
	}

	/* Distancia: ahora es un termino mas, no una compuerta. */
	w = profile->engageWeightDistance;
	if ( w > 0.0f && profile->engageNeutralDist > 0.0f ) {
		term = BotLite_ClampUnit( ( profile->engageNeutralDist - snapshot->dist ) / profile->engageNeutralDist );
		total += term * w;
		weightSum += w;
	}

	/* Lo que viene funcionando. */
	w = profile->engageWeightEffect;
	if ( w > 0.0f ) {
		term = BotLite_ClampUnit( info->engage.meleeEffect - info->engage.rangedEffect );
		total += term * w;
		weightSum += w;
	}

	if ( weightSum <= 0.0f ) {
		return 0.0f;
	}
	return total / weightSum;
}

void BotLite_UpdateEngageIntent( gentity_t *bot, int clientNum, const botlite_snapshot_t *snapshot ) {
	botlite_info_t *info;
	const botlite_profile_t *profile;
	botlite_engage_intent_t desired;
	float score;
	float magnitude;

	if ( !bot || !snapshot || !BotLite_EngageIntentActive( clientNum ) ) {
		return;
	}

	info = &g_botlite[clientNum];
	profile = info->profile ? info->profile : BotLite_GetProfile( info->skill );
	if ( !profile ) {
		return;
	}

	if ( !snapshot->hasTarget ) {
		BotLite_ResetEngageState( clientNum, qfalse );
		return;
	}

	BotLite_SampleEngageEffect( clientNum, snapshot );

	/* --- Anulaciones duras. No son preferencias: son limites del motor. --- */

	/* Dentro de un lock de melee el arma no dispara: PM_WeaponRelease y return
		 * incondicionales en bg_pmove.c:2742. Querer ranged aca no significa nada. */
	if ( snapshot->botInMelee ) {
		if ( info->engage.intent != BOTLITE_ENGAGE_MELEE ) {
			info->engage.intent = BOTLITE_ENGAGE_MELEE;
			info->engage.nextSwitchTime = level.time + profile->engageSwitchMinMs;
			BotLite_DebugLog( bot, "Engage intent -> MELEE (forzado: lock de melee)" );
		}
		return;
	}

	/* Sin linea de vista no hay tiro posible: hay que acercarse igual. */
	if ( !snapshot->hasLineOfSight ) {
		if ( info->engage.intent != BOTLITE_ENGAGE_MELEE ) {
			info->engage.intent = BOTLITE_ENGAGE_MELEE;
			info->engage.nextSwitchTime = level.time + profile->engageSwitchMinMs;
			BotLite_DebugLog( bot, "Engage intent -> MELEE (forzado: sin linea de vista)" );
		}
		return;
	}

	score = BotLite_ComputeEngageScore( clientNum, snapshot, profile );
	info->engage.lastScore = score;
	desired = ( score >= 0.0f ) ? BOTLITE_ENGAGE_MELEE : BOTLITE_ENGAGE_RANGED;

	if ( info->engage.intent == BOTLITE_ENGAGE_NONE ) {
		info->engage.intent = desired;
		info->engage.nextSwitchTime = level.time + profile->engageSwitchMinMs;
		BotLite_DebugLog( bot, va( "Engage intent -> %s score=%.2f (inicial)",
			BotLite_EngageIntentName( desired ), score ) );
		return;
	}

	if ( desired == info->engage.intent ) {
		return;
	}
	if ( level.time < info->engage.nextSwitchTime ) {
		return;
	}

	/* Margen: no alcanza con cruzar el cero, hay que cruzarlo con conviccion.
		 * Sin esto el bot se pasaria la pelea cambiando de idea en el borde. */
	magnitude = ( score < 0.0f ) ? -score : score;
	if ( magnitude < profile->engageSwitchMargin ) {
		return;
	}

	info->engage.intent = desired;
	info->engage.nextSwitchTime = level.time + profile->engageSwitchMinMs;
	info->engage.kiteUntil = 0;
	BotLite_DebugLog( bot, va( "Engage intent -> %s score=%.2f ki=%d%% sta=%d%% hp=%d%% dist=%.0f tgt[chg=%d blk=%d kb=%d]",
		BotLite_EngageIntentName( desired ), score,
		BotLite_KiPercent( clientNum ),
		BotLite_RealStaminaEnabled() ? BotLite_StaminaPercent( clientNum ) : 100,
		BotLite_HealthPercent( clientNum ),
		snapshot->dist,
		snapshot->targetCharging ? 1 : 0,
		snapshot->targetBlocking ? 1 : 0,
		snapshot->targetKnockbackTime > 0 ? 1 : 0 ) );
}

/*
 * Kiting: la intencion es disparar pero el rival esta encima.
 *
 * Alcance real: el lock de melee solo se sostiene por debajo de 64 unidades
 * (bg_pmove.c:2362 rompe el lock al pasarse). Estando LOCKEADO el bot no puede
 * disparar (bg_pmove.c:2742) y la intencion se fuerza a MELEE, asi que el kiting
 * no aplica ahi -- soltarse de un lock es trabajo del zanzoken defensivo (T2.5).
 * Lo que si cubre esta funcion es la franja de "cerca pero suelto": el rival
 * encima a 100-900 unidades, donde retroceder alcanza para poder tirar.
 *
 * Devuelve qtrue mientras esta abriendo hueco (el frame se consume en eso).
 */
qboolean BotLite_EngageWantsKite( int clientNum, const botlite_snapshot_t *snapshot ) {
	botlite_info_t *info;
	const botlite_profile_t *profile;
	const botlite_combat_policy_t *policy;

	if ( !snapshot || clientNum < 0 || clientNum >= BOTLITE_MAX_BOTS ) {
		return qfalse;
	}

	info = &g_botlite[clientNum];
	policy = BotLite_GetCombatPolicy( info->skill );
	profile = info->profile ? info->profile : BotLite_GetProfile( info->skill );
	if ( !policy || !policy->allowsEngageKiting || !profile ) {
		return qfalse;
	}
	if ( profile->engageKiteMinDist <= 0.0f ) {
		return qfalse;
	}
	if ( info->engage.intent != BOTLITE_ENGAGE_RANGED ) {
		return qfalse;
	}
	/* Dentro de un lock de melee esto no aplica: el motor no deja disparar ahi
		 * (bg_pmove.c:2742) y la intencion ya se fuerza a MELEE. */
	if ( snapshot->botInMelee ) {
		return qfalse;
	}

	/* Histeresis. Si ya venia abriendo hueco, el umbral de salida es mas lejos
		 * que el de entrada; si no, entra con el umbral normal. */
	if ( level.time < info->engage.kiteUntil ) {
		return ( snapshot->dist < profile->engageKiteMinDist * BOTLITE_ENGAGE_KITE_EXIT_SCALE ) ? qtrue : qfalse;
	}
	return ( snapshot->dist < profile->engageKiteMinDist ) ? qtrue : qfalse;
}

qboolean BotLite_RunRangedKite( gentity_t *bot, int clientNum, gentity_t *target, const botlite_snapshot_t *snapshot ) {
	botlite_info_t *info;
	const botlite_profile_t *profile;

	if ( !bot || !target || !snapshot ) {
		return qfalse;
	}
	if ( !BotLite_EngageWantsKite( clientNum, snapshot ) ) {
		g_botlite[clientNum].engage.kiteUntil = 0;
		return qfalse;
	}

	info = &g_botlite[clientNum];
	profile = info->profile ? info->profile : BotLite_GetProfile( info->skill );
	info->engage.kiteUntil = level.time + BOTLITE_ENGAGE_KITE_GRACE_MS;

	/*
	 * Se SUELTA el lock mientras retrocede, a proposito.
	 *
	 * PM_Melee corre entera con solo tener lockedTarget > 0 (bg_pmove.c:2336), y
	 * con forwardmove < 0 pone al bot en stMeleeDegressing (bg_pmove.c:2354) --
	 * un estado que ningun bot habia producido nunca antes de esta fase, porque
	 * ninguno retrocedia con el lock puesto. En la primera prueba en juego el
	 * proceso murio en el unico frame del log en que el bot aparece en DEGRESS.
	 * NO esta demostrado que sea la causa; es una correlacion de una sola muestra.
	 *
	 * Soltar el lock evita el estado por completo y no cuesta nada: el bot no lo
	 * necesita para disparar, y BotLite_RunRangedPressure lo vuelve a tomar en
	 * cuanto el hueco esta abierto. Si el crash reaparece, al menos queda
	 * descartada esta hipotesis.
	 */
	BotLite_ClearLock( bot );
	BotLite_FaceTarget( bot, target );
	BotLite_EA_MoveBack( bot, 127 );
	BotLite_EA_BoostIfAllowed( bot, clientNum );

	if ( level.time >= info->engage.lastLogTime ) {
		info->engage.lastLogTime = level.time + 900;
		BotLite_DebugLog( bot, va( "Engage kite: abriendo hueco dist=%.0f entra<%.0f sale>=%.0f",
			snapshot->dist, profile->engageKiteMinDist,
			profile->engageKiteMinDist * BOTLITE_ENGAGE_KITE_EXIT_SCALE ) );
	}
	return qtrue;
}
