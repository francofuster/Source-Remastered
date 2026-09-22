#include "g_local.h"
#include "g_botlite.h"

/*
 * ============================================================================
 * FASE 5 -- Esquivar ataques entrantes
 * ============================================================================
 *
 * Hasta aca el bot solo reaccionaba a lo que YA lo estaba tocando (isStruggling,
 * dano recibido, knockback). No tenia ninguna percepcion de proyectiles en vuelo.
 * Esto agrega anticipacion.
 *
 * Por que se puede hacer server-side sin trucar nada: el bot corre en el game
 * module y ve los gentity_t completos, no la vista replicada de un cliente. Los
 * proyectiles son entidades normales con toda su fisica expuesta:
 *   - s.pos.trBase / s.pos.trDelta -- trayectoria
 *   - r.ownerNum                   -- quien disparo (para ignorar los propios)
 *   - s.eFlags & EF_GUIDED         -- si persigue al objetivo
 *
 * La prediccion usa el punto de maxima aproximacion entre una recta (el
 * proyectil) y un punto (el bot), resuelto de forma analitica. Es geometria
 * basica, no una simulacion paso a paso: mucho mas barato por frame.
 *
 * LIMITE CONOCIDO -- proyectiles guiados: un EF_GUIDED corrige su rumbo hacia el
 * objetivo, asi que extrapolar su trDelta en linea recta da una respuesta
 * equivocada (parece que va a errar cuando en realidad va a doblar). Para esos
 * no se predice trayectoria: se usa proximidad simple, que es la unica senal
 * honesta disponible sin duplicar la logica de homing del motor.
 */

/* Radio de impacto efectivo. Generoso a proposito: errar por poco igual duele
 * por el splash de la mayoria de los ataques. */
#define BOTLITE_DODGE_HIT_RADIUS		140.0f
/* Solo interesan amenazas que lleguen dentro de esta ventana. Mas alla hay
 * tiempo de sobra para reaccionar despues y no vale gastar la decision. */
#define BOTLITE_DODGE_LOOKAHEAD_MS		1200
/* Para guiados: radio de proximidad cruda, ya que no se puede extrapolar. */
#define BOTLITE_DODGE_GUIDED_RADIUS		900.0f
/* Debajo de esto la amenaza es tan inmediata que solo el zanzoken llega. */
#define BOTLITE_DODGE_ZANZOKEN_MS		350
/*
 * Piso de reaccion: por debajo de esto no hay esquive posible y solo se gasta
 * el recurso.
 *
 * Medido en la primera sesion en que la Fase 5 llego a activarse: de 14 esquives
 * detectados, 10 fueron zanzoken con eta de 5, 6, 8, 12, 16, 54, 58, 71, 148 y
 * 234 ms. Un boton apretado ahora se aplica recien en el ClientThink siguiente
 * (~50ms de frame de servidor), asi que con eta de 5ms el proyectil impacta
 * antes de que el teleport empiece: era stamina tirada a la basura.
 *
 * No es un bug de la prediccion -- es que el combate real de ZEQ2 pasa a corta
 * distancia y los haces son rapidos, asi que el vuelo entero dura menos que la
 * ventana de 1200ms que el modulo mira. La respuesta correcta no es esquivar
 * antes, es reconocer que ese golpe ya no se puede evitar.
 */
#define BOTLITE_DODGE_MIN_REACTION_MS	120

typedef struct {
	gentity_t *missile;
	float timeToImpactMs;
	qboolean guided;
	vec3_t approachDir;		/* direccion de viaje del proyectil */
} botlite_threat_t;

static qboolean BotLite_MissileIsHostile( gentity_t *bot, gentity_t *missile ) {
	if ( !missile->inuse ) {
		return qfalse;
	}
	if ( missile->s.eType != ET_MISSILE && missile->s.eType != ET_BEAMHEAD ) {
		return qfalse;
	}
	/* Los propios no cuentan. */
	if ( missile->r.ownerNum == bot->s.number ) {
		return qfalse;
	}
	if ( missile->r.ownerNum >= 0 && missile->r.ownerNum < level.maxclients ) {
		if ( OnSameTeam( bot, &g_entities[missile->r.ownerNum] ) ) {
			return qfalse;
		}
	}
	return qtrue;
}

/*
 * Punto de maxima aproximacion de una recta a un punto.
 *
 * El proyectil esta en P con velocidad V; el bot en B. La distancia al cuadrado
 * en funcion del tiempo es |P + V*t - B|^2, una parabola en t. Su minimo esta en
 * t = -( (P-B) . V ) / |V|^2. Si ese t es negativo el proyectil ya paso de largo
 * y se esta alejando: no es amenaza.
 *
 * Devuelve qtrue y llena outTimeMs si el proyectil va a pasar dentro del radio.
 */
static qboolean BotLite_PredictLinearImpact( gentity_t *bot, gentity_t *missile,
											 float *outTimeMs, vec3_t outDir ) {
	vec3_t missilePos;
	vec3_t missileVel;
	vec3_t rel;
	vec3_t closest;
	float speedSq;
	float t;
	float distSq;

	BG_EvaluateTrajectory( &missile->s, &missile->s.pos, level.time, missilePos );
	BG_EvaluateTrajectoryDelta( &missile->s, &missile->s.pos, level.time, missileVel );

	speedSq = DotProduct( missileVel, missileVel );
	if ( speedSq < 1.0f ) {
		return qfalse;
	}

	VectorSubtract( missilePos, bot->client->ps.origin, rel );
	t = -DotProduct( rel, missileVel ) / speedSq;

	/* Ya paso de largo. */
	if ( t <= 0.0f ) {
		return qfalse;
	}
	if ( t * 1000.0f > BOTLITE_DODGE_LOOKAHEAD_MS ) {
		return qfalse;
	}

	VectorMA( rel, t, missileVel, closest );
	distSq = DotProduct( closest, closest );
	if ( distSq > BOTLITE_DODGE_HIT_RADIUS * BOTLITE_DODGE_HIT_RADIUS ) {
		return qfalse;
	}

	*outTimeMs = t * 1000.0f;
	VectorCopy( missileVel, outDir );
	VectorNormalize( outDir );
	return qtrue;
}

/*
 * Busca la amenaza mas inminente. Devuelve qtrue si hay alguna.
 */
static qboolean BotLite_FindIncomingThreat( gentity_t *bot, botlite_threat_t *out ) {
	int i;
	gentity_t *ent;
	float timeMs;
	vec3_t dir;
	qboolean found;

	found = qfalse;
	out->timeToImpactMs = 999999.0f;

	/* Los primeros MAX_CLIENTS slots son jugadores: los proyectiles viven arriba. */
	for ( i = MAX_CLIENTS; i < level.num_entities; i++ ) {
		ent = &g_entities[i];
		if ( !BotLite_MissileIsHostile( bot, ent ) ) {
			continue;
		}

		if ( ent->s.eFlags & EF_GUIDED ) {
			/* Guiado: no se puede extrapolar en recta porque va a corregir rumbo.
			 * Se usa proximidad cruda, que es la unica senal honesta. */
			vec3_t delta;
			float dist;

			VectorSubtract( ent->r.currentOrigin, bot->client->ps.origin, delta );
			dist = VectorLength( delta );
			if ( dist > BOTLITE_DODGE_GUIDED_RADIUS ) {
				continue;
			}
			/* Urgencia proporcional a la cercania, para poder compararlo con las
			 * amenazas de trayectoria calculada en la misma escala. */
			timeMs = ( dist / BOTLITE_DODGE_GUIDED_RADIUS ) * BOTLITE_DODGE_LOOKAHEAD_MS;
			if ( timeMs < out->timeToImpactMs ) {
				out->missile = ent;
				out->timeToImpactMs = timeMs;
				out->guided = qtrue;
				VectorNegate( delta, out->approachDir );
				VectorNormalize( out->approachDir );
				found = qtrue;
			}
			continue;
		}

		if ( !BotLite_PredictLinearImpact( bot, ent, &timeMs, dir ) ) {
			continue;
		}
		if ( timeMs < out->timeToImpactMs ) {
			out->missile = ent;
			out->timeToImpactMs = timeMs;
			out->guided = qfalse;
			VectorCopy( dir, out->approachDir );
			found = qtrue;
		}
	}

	return found;
}

/*
 * Esquive lateral: moverse perpendicular a la trayectoria entrante.
 *
 * Se elige el lado por el producto cruz con el eje vertical, y se desempata
 * mirando hacia donde ya apunta el bot -- asi el esquive no lo obliga a girar
 * en redondo, que lo dejaria mas expuesto que quedarse quieto.
 */
static int BotLite_DodgeSidewaysCommand( gentity_t *bot, const botlite_threat_t *threat ) {
	vec3_t up = { 0.0f, 0.0f, 1.0f };
	vec3_t side;
	vec3_t forward;
	vec3_t right;

	CrossProduct( threat->approachDir, up, side );
	if ( VectorNormalize( side ) < 0.1f ) {
		/* Trayectoria casi vertical: cualquier lado sirve. */
		return 127;
	}

	AngleVectors( bot->client->ps.viewangles, forward, right, NULL );
	return ( DotProduct( side, right ) >= 0.0f ) ? 127 : -127;
}

static void BotLite_DodgeSideways( gentity_t *bot, const botlite_threat_t *threat ) {
	BotLite_EA_MoveRight( bot, BotLite_DodgeSidewaysCommand( bot, threat ) );
}

/*
 * T5.3 -- Decidir si conviene aguantar el golpe y seguir presionando en vez de
 * esquivar.
 *
 * Reutiliza la misma nocion de ventaja que ya usan T2.7 y T3.5 en vez de
 * inventar una nueva: con ventaja clara de vida, interrumpir la propia ofensiva
 * para esquivar cuesta mas de lo que ahorra.
 */
static qboolean BotLite_ShouldTankAndCounter( gentity_t *bot, const botlite_threat_t *threat ) {
	int botHealth;
	int enemyHealth;

	if ( !bot->client->ps.lockedPlayer ) {
		return qfalse;
	}
	/* Una amenaza muy inminente no se aguanta "por decision": ya no hay opcion. */
	if ( threat->timeToImpactMs < BOTLITE_DODGE_ZANZOKEN_MS ) {
		return qfalse;
	}

	botHealth = bot->client->ps.powerLevel[plHealth];
	enemyHealth = bot->client->ps.lockedPlayer->powerLevel[plHealth];
	if ( enemyHealth <= 0 ) {
		return qfalse;
	}

	/* Con vida claramente superior, mantener la presion vale mas que el esquive. */
	return ( botHealth > enemyHealth * 2 ) ? qtrue : qfalse;
}

qboolean BotLite_RunDodgeIncoming( gentity_t *bot, int clientNum, const botlite_snapshot_t *snapshot ) {
	botlite_info_t *info;
	const botlite_combat_policy_t *policy;
	botlite_threat_t threat;

	if ( !bot || !bot->client || !snapshot ) {
		return qfalse;
	}

	info = &g_botlite[clientNum];
	policy = BotLite_GetCombatPolicy( info->skill );
	if ( !policy || !policy->allowsDodgeIncoming ) {
		return qfalse;
	}

	/* Estados donde el bot no controla su movimiento: esquivar es imposible. */
	if ( snapshot->botDisabled || snapshot->botFrozen || snapshot->botStruggling ) {
		return qfalse;
	}
	if ( level.time < info->melee.dodgeIncomingNextTime ) {
		return qfalse;
	}

	memset( &threat, 0, sizeof( threat ) );
	if ( !BotLite_FindIncomingThreat( bot, &threat ) ) {
		return qfalse;
	}

	/* Ya no hay nada que hacer con este: ver BOTLITE_DODGE_MIN_REACTION_MS. Se
	 * deja pasar el frame para que la tactica normal siga corriendo, y se pone un
	 * cooldown corto para no reevaluar la misma amenaza inevitable cada tick. */
	if ( threat.timeToImpactMs < (float)BOTLITE_DODGE_MIN_REACTION_MS ) {
		info->melee.dodgeIncomingNextTime = level.time + 200;
		return qfalse;
	}

	if ( BotLite_ShouldTankAndCounter( bot, &threat ) ) {
		BotLite_DebugLog( bot, va( "Incoming threat: holding ground (eta=%dms guided=%d)",
			(int)threat.timeToImpactMs, threat.guided ? 1 : 0 ) );
		info->melee.dodgeIncomingNextTime = level.time + 600;
		return qfalse;
	}

	/*
	 * Zanzoken cuando la ventana es corta, o SIEMPRE si la amenaza es guiada.
	 *
	 * Lo segundo sale de mirar el log: la mayoria de los esquives detectados eran
	 * guided=1 y casi todos salieron como SIDESTEP, porque su eta sintetica (por
	 * proximidad) suele superar el umbral. Contra un proyectil guiado un paso
	 * lateral no sirve: EF_GUIDED es un haz que el que dispara sigue dirigiendo
	 * (g_usermissile.c:952-953), asi que corrige y acompana el desplazamiento. Lo
	 * unico que lo pierde es un salto grande e instantaneo. Eso explica el "a
	 * veces esquivan y a veces no" reportado en juego.
	 *
	 * Respeta la misma restriccion del motor que T2.5 (nada de zanzoken dentro
	 * de melee).
	 */
	if ( ( threat.timeToImpactMs < BOTLITE_DODGE_ZANZOKEN_MS || threat.guided ) &&
		 !snapshot->botInMelee &&
		 ( snapshot->botActionFlags & BOTACT_CAN_ZANZOKEN ) &&
		 BotLite_StaminaAllowsSpend( clientNum, BOTLITE_SPEND_NORMAL ) ) {
		info->melee.dodgeIncomingNextTime = level.time + 1400;
		BotLite_DebugLog( bot, va( "Dodge incoming: ZANZOKEN eta=%dms guided=%d",
			(int)threat.timeToImpactMs, threat.guided ? 1 : 0 ) );
		/* Lateral, no hacia adelante: el zanzoken escala la velocidad existente, y
			 * teletransportarse a lo largo de la trayectoria del proyectil no esquiva
			 * nada. rightmove tampoco dispara stMeleeDegressing, a diferencia de
			 * retroceder. */
		BotLite_StartZanzoken( bot, clientNum, 0,
			BotLite_DodgeSidewaysCommand( bot, &threat ), 0 );
		return qtrue;
	}

	info->melee.dodgeIncomingNextTime = level.time + 500;
	BotLite_DebugLog( bot, va( "Dodge incoming: SIDESTEP eta=%dms guided=%d",
		(int)threat.timeToImpactMs, threat.guided ? 1 : 0 ) );
	BotLite_DodgeSideways( bot, &threat );
	BotLite_EA_BoostIfAllowed( bot, clientNum );
	return qtrue;
}
