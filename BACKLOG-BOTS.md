# Backlog de desarrollo — Rediseño de skills de bots (BotLite)

> Objetivo: pasar de 3 skills que son variaciones numéricas del mismo guión, a 3 skills donde
> **cada nivel desbloquea una capa de decisión que el anterior no tiene**.
> Contexto de arquitectura: ver [INDEX.md](INDEX.md).

## Principio rector

El bot hoy **ejecuta un guión**; queremos que **lea el estado del rival y responda**.
Toda tarea de comportamiento debe poder responder: *¿qué dato del rival dispara esta decisión?*
Si la respuesta es "un timer aleatorio", la tarea está mal planteada.

## Mecánica clave: el ciclo ki ↔ stamina ↔ visibilidad

Verificado en `bg_pmove.c:636-648` y `g_radar.c` / `cg_radar.c`. Es el sistema que hace que la
gestión de recursos sea una decisión táctica y no una barra que se rellena sola.

**Fórmula de recuperación de stamina** (`bg_pmove.c:636-646`):

```
recovery = plMaximum * 0.01
         * idleScale      // 2.8 si está COMPLETAMENTE quieto, 1 si se mueve
         * statScale      // 1.0 - (plCurrent/plMaximum), clamp [0.25, 0.75]
         * fatigueScale   // plFatigue/plMaximum, piso 0.15
         * stFatigueRecovery
```

Tres consecuencias que definen el comportamiento:

1. **Bajar el ki acelera la recuperación de stamina hasta 3x.** `statScale` es *inverso* al ki
   actual: con ki al máximo vale 0.25, con ki al mínimo vale 0.75.
2. **Quedarse quieto la multiplica por 2.8.** Moverse aunque sea un poco mata el bonus.
3. **`fatigueScale` crea una espiral de muerte.** Cuanto más agotado, más lento recupera
   (piso 0.15). Dejar que la stamina toque fondo es una trampa difícil de revertir.

**La recuperación se ANULA por completo** (`recovery = 0`) mientras esté activo cualquiera de:
`usingAlter` · `isStruggling` · `usingSoar` · `isBreakingLimit` · `weaponstate >= WEAPON_GUIDING`.

`usingAlter` se activa mientras se **mantiene** el botón de ki (en cualquier dirección). Por lo
tanto **la recuperación óptima es pulsada, no sostenida**: bajar ki a golpes cortos, soltar para
que corra el `recovery` con `statScale` ya alto, repetir. Un bot que mantenga el botón apretado
recupera **cero**. Este es el detalle más fácil de implementar mal.

**Curarse cuesta stamina** (`bg_pmove.c:722-744`): al llegar a ki máximo y seguir cargando se
entra en `isBreakingLimit`, que convierte `healthPool` en vida — pero suma `plUseFatigue +=
raise * 0.25` *y* anula el `recovery`. Es una quema neta de stamina. Además deja el ki al
máximo, lo que baja `statScale` a 0.25 y enlentece la recuperación posterior. Ese es el
equilibrio real a tunear: **curarse demasiado deja al bot sano pero exhausto y lento**.

**Visibilidad en el radar** — el ki es también la firma de detección:

| Situación | Efecto en el radar |
|---|---|
| `plCurrent <= 0` | el servidor no emite la posición (`g_radar.c:33`) — inalcanzable: bajar ki tiene piso 1 (`bg_pmove.c:705`) |
| ki bajo respecto al máximo | el blip se desvanece: su alfa es `plCurrent/plMaximum` (`cg_radar.c`) → **con ki al mínimo es prácticamente invisible** |
| cargando ki (`EF_AURA`) | marca `RADAR_BURST`: un resaltado brillante (`g_radar.c:48`) |
| cargando un ataque ≥50% | marca `RADAR_WARN` |

O sea: **el ocultamiento es un gradiente, no un interruptor**, y cargar ki para recuperarse es
exactamente lo que delata la posición. Esa tensión es el corazón de las tareas T2.8 y T3.6.

> **Asimetría a respetar:** los bots **no reciben datos de radar** — `g_radar.c` los excluye
> explícitamente (`if ( ent->r.svFlags & SVF_BOT ) continue;`). Un bot puede esconderse del
> jugador, pero no puede usar el radar para encontrar a un jugador escondido. No darle al bot
> un sentido que el jugador no puede contrarrestar: que dependa de LOS y distancia como hoy.

## Reparto de capacidades por skill

| Capacidad | Skill 1 | Skill 2 | Skill 3 |
|---|:---:|:---:|:---:|
| Melee Speed (taps) | ✅ | ✅ | ✅ |
| Block reactivo a carga rival | ✅ | ✅ | ✅ |
| Forcejeo de haces (boost) | ✅ ki≥75% | ✅ ki≥40% | ✅ ki≥20% |
| Gestión mínima de stamina (no gastar en crítico) | ✅ | ✅ | ✅ |
| **Transformación** | ❌ **nunca** | ✅ multi-tier dosificado | ✅ agresiva |
| Cargas Power / Stun **deliberadas** | ❌ nunca | ✅ | ✅ |
| Breakers **deliberados** | ❌ (hoy salen por accidente) | ✅ | ✅ |
| Carga de ki activa entre intercambios | ❌ | ✅ | ✅ |
| Zanzoken defensivo (escape) | ❌ | ✅ | ✅ |
| Ball-flip (escape de knockback propio) | ❌ | ✅ | ✅ rápido |
| Presupuesto de stamina táctico | ❌ | ✅ | ✅ |
| **Decisión pelear / huir / recuperar** | ❌ | ✅ | ✅ |
| **Ciclo de recuperación ki↔stamina** | ❌ | ✅ | ✅ |
| Evade | ❌ | ❌ | ✅ |
| Lectura de patrón del rival | ❌ | ❌ | ✅ |
| **Punish a distancia tras knockback** | ❌ | ❌ | ✅ |
| Break-limit ofensivo | ❌ | ❌ | ✅ |
| Economía vida / ki / stamina | ❌ | ❌ | ✅ |
| **Ocultamiento defensivo por ki mínimo** | ❌ | ❌ | ✅ solo supervivencia |
| **Esquivar proyectiles entrantes** (Fase 5) | ❌ | ✅ | ✅ |
| **Elegir melee/ranged por decisión** (Fase 6) | ❌ por distancia | ✅ lento | ✅ + kiting |

Skill 1 nunca se transforma: pelea siempre en tier base. Es el techo de dificultad bajo deliberado.

---

# FASE 0 — Cimientos

Nada de la Fase 1-3 se puede construir sin esto. Son tareas de infraestructura, sin cambio
visible de comportamiento (salvo T0.2).

### T0.1 — Ampliar el snapshot de percepción ✅ HECHO
**Bloquea:** prácticamente todo lo demás.
**Archivos:** `Game/Game/g_botlite.h`, `Game/Game/g_botlite_target.c`

Hoy `botlite_snapshot_t` tiene `targetCharging`/`targetBlocking` pero **no expone el dato
más importante del sistema de melee**: `target->client->ps.stats[stMeleeState]`, que dice
exactamente qué está haciendo el rival (Speed / Power / Stun / Block / Evade / Breaker).

Agregar al snapshot:
- `int targetMeleeState` — valor crudo de `stats[stMeleeState]` del rival
- `int botMeleeState` — el propio, para saber en qué estado quedamos
- `int targetKnockbackTime` — `target->client->ps.timers[tmKnockback]` (hoy solo se usa
  conflacionado dentro de `targetCrashPartial`, ver `g_botlite_target.c:517`)
- `qboolean targetStruggling` / `botStruggling` — `bitFlags & isStruggling`
- `int botFatigue`, `int botFatigueMax`, `int targetFatigue` — para T0.3
- `int botTier`, `int targetTier` — `powerLevel[plTierCurrent]`
- `int botMeleeChargeTime` — `timers[tmMeleeCharge]`, para saber cuánto llevamos cargando

**Criterio de aceptación:** los campos se pueblan correctamente y quedan visibles en el log
de debug (`/addbot <char> -debug`). Cero cambio de comportamiento.
**Riesgo:** bajo.

---

### T0.2 — Forcejeo de haces (power struggle) ✅ HECHO
**Depende de:** T0.1
**Archivos:** nuevo `Game/Game/g_botlite_struggle.c`, `g_botlite.h`, `g_botlite_action.c`,
`g_botlite_ai.c`, `Makefile`, `game.bat`, `game.q3asm`

Hoy `isStruggling` **no aparece en ningún `g_botlite_*.c`**. Cuando un rayo grande atrapa al
bot (`g_usermissile.c:1631-1639`), el bot se queda quieto forcejeando sin resistir hasta
perder. Es daño gratis contra cualquier skill.

> **Corrección sobre la suposición inicial:** no es "mashear un botón de escape". El struggle
> es un **choque de haces** (dos `ET_BEAMHEAD` colisionando) y se resuelve por potencia, no por
> velocidad de pulsación. Verificado en `g_usermissile.c:1629-1646` y `:443`.

Mecánica real, con una consecuencia de diseño fuerte:

- `Think_NormalMissileStruggle` (`g_usermissile.c:443`) **solo** suma potencia al haz propio si
  el dueño tiene `usingBoost` activo **y** `powerLevel[plCurrent] > 1`. Es la única entrada del
  jugador que altera el resultado.
- `PM_CheckBoost` (`bg_pmove.c:811`) hace **early-return mientras `isStruggling`**. El estado de
  boost queda **congelado** en el instante del choque: no se puede empezar *ni dejar* de boostear
  una vez iniciado el forcejeo.

→ **La decisión se toma ANTES del choque.** Un bot que no venía boosteando mientras cargaba el
haz pierde el forcejeo haga lo que haga después. Por eso el módulo actúa en dos momentos:
`BotLite_StrugglePrepare()` (engancha el boost mientras hay un haz activo) y
`BotLite_RunStruggle()` (mantiene la postura y no malgasta el ki que sostiene el bonus).

**Bloqueador encontrado:** `BotLite_SanitizeButtons` (`g_botlite_action.c:28`) descartaba
`BUTTON_BOOST` siempre que hubiera `ATTACK`/`ALT_ATTACK`. Eso le impedía al bot **ganar cualquier
forcejeo** y además le quitaba el **2x de velocidad de carga** que da boostear mientras se carga
(`bg_pmove.c:2834`). Resuelto con una excepción explícita (`allowBoostWithAttack`) que solo se
habilita cuando este módulo la pide, para no alterar el resto del combate.

Compromiso por skill (umbral de ki para disputar el forcejeo): skill 1 ≥75%, skill 2 ≥40%,
skill 3 ≥20%.

**Criterio de aceptación:** un bot con ki suficiente gana forcejeos de forma observable;
skill 1 los disputa mucho menos que skill 3. El log de debug muestra `Struggle started ...
boosted=1` cuando corresponde.
**Riesgo:** bajo — módulo nuevo y aislado; el único cambio a código existente es la excepción
opt-in del sanitizador, que sin la bandera se comporta igual que antes.
**Por qué primero:** máxima ganancia percibida por mínimo esfuerzo, y sin riesgo de regresión.

---

### T0.3 — Helper central de presupuesto de stamina ✅ HECHO
**Depende de:** T0.1
**Archivos:** nuevo `Game/Game/g_botlite_resource.c`, `g_botlite.h`, `Makefile`

Crear `BotLite_StaminaBudget( clientNum )` que devuelva un estado clasificado:

| Estado | Umbral orientativo | Política |
|---|---|---|
| `HOLGADO` | > 70% | gasta libremente (boost, zanzoken, cargas) |
| `AJUSTADO` | 30-70% | solo gasta si la acción es decisiva |
| `CRÍTICO` | < 30% | no gasta; busca desengancharse y recuperar |
| `AGOTADO` | < 10% | prioridad absoluta: retirarse y cargar |

Todas las decisiones de gasto (boost, zanzoken, cargas de melee, transformación, ataques de
ki) deben consultar este helper en vez de decidir por su cuenta. **Un solo lugar donde vive
la política de recursos.**

Mientras el override de fatiga siga activo (T0.4 sin flipear), el helper devuelve siempre
`HOLGADO` — así se puede integrar en todo el código sin cambiar comportamiento todavía.

Este helper no solo *gatea gastos*: es el disparador del cambio de modo a `RECOVER` (T2.7) y
el que define los objetivos del ciclo de recuperación (T2.8). Diseñarlo pensando en eso desde
el principio evita rehacerlo.

**Criterio de aceptación:** helper implementado y consultado desde los puntos de gasto;
con el cvar de T0.4 en 0, el comportamiento es idéntico al actual.
**Riesgo:** bajo mientras el override siga puesto.

**Nota:** los cálculos de porcentaje usan float a propósito. `g_powerlevelMaximum` es un cvar
de usuario (default 32767) y con un tope alto la forma entera `valor * 100` desbordaría el int
de 32 bits del VM.

**Estado:** implementado en `g_botlite_resource.c` con `BotLite_StaminaBudget()`,
`BotLite_StaminaAllowsSpend( clientNum, cost )` y los niveles de costo
`CHEAP` / `NORMAL` / `EXPENSIVE`. Ya consultado desde `BotLite_ShouldUseBoost` (CHEAP),
`BotLite_ShouldUseSanzoken` (EXPENSIVE) y `BotLite_StrugglePrepare` (CHEAP). El presupuesto
aparece en el log de debug dentro de la línea `SNAP self ... fatigue=NN%(ESTADO)`.

---

### T0.4 — Reactivar la stamina real (detrás de cvar) ✅ HECHO (cvar en 0)
**Depende de:** T0.3
**Archivos:** `Game/Game/bg_pmove.c`, `g_main.c`, `g_local.h`, `Shared/q_shared.h`,
`g_botlite_resource.c`, `g_botlite_ai.c`
**⚠️ Tarea de mayor riesgo del proyecto.**

> **Detalle de implementación:** `bg_pmove.c` se compila **también en el cgame**
> (`cgame.q3asm:21`), así que no puede leer un cvar `g_*`. El puente es un bit nuevo de
> `playerState.options`, `PSO_BOT_STAMINA` (`Shared/q_shared.h`), que el servidor sincroniza
> cada frame desde `BotLite_SyncStaminaMode()`. El helper pasó a llamarse
> `PM_BotFatigueExempt()`: devuelve `qtrue` solo si el cliente es bot **y** el bit no está
> puesto, de modo que con `g_botStamina 0` el comportamiento es idéntico al anterior.

Quitar/gatear el override `PM_IsBotControlled()` que fuerza `plFatigue = plMaximum` en los
**tres** puntos: `bg_pmove.c:346-352`, `:369-372`, `:655-669`.

Introducir cvar `g_botStamina` (0 = override actual, 1 = stamina real). **Default 0** hasta
terminar la Fase 2; recién ahí se flipea a 1 y se re-tunea.

Efectos en cascada a validar uno por uno al flipear:

| Mecánica | Ref. | Qué observar |
|---|---|---|
| Velocidad de movimiento | `bg_pmove.c:998` | el bot se vuelve más lento al cansarse |
| Zanzoken bloqueado | `:296` | con fatiga ≤ 1 deja de poder teletransportarse |
| Distancia de zanzoken | `:330` | escala con fatiga |
| Daño de Speed melee | `:2568` | escala con fatiga |
| Defensa contra ki | `:433` | escala con fatiga |
| **Fatiga negativa drena vida** | `:375-381` | un bot que se sobre-exige se auto-mata |
| Ball-flip cuesta fatiga | `:248` | sin fatiga no puede escapar del knockback |
| Sustain de tier | `g_tiers.c:73` | **se des-transforma solo** si la fatiga cae |

**Criterio de aceptación:** con `g_botStamina 1` el bot completa un combate de 3 minutos sin
auto-matarse por fatiga negativa y sin quedar permanentemente inmóvil. Con `g_botStamina 0`
el comportamiento es byte-idéntico al actual.
**Riesgo:** alto. No mergear sin la matriz de pruebas de T4.2.

---

# FASE 1 — Skill 1: Reactivo básico

Piso más alto que hoy, pero sigue siendo legible y vencible.

### T1.1 — Reemplazar el combo RNG por melee reactivo ✅ HECHO
**Depende de:** T0.1
**Archivos:** `Game/Game/g_botlite_melee.c`

Hoy `BotLite_StartCombo`/`BotLite_AdvanceCombo` (`g_botlite_melee.c:27-90`) es una secuencia
fija con `rand()` que nunca mira al rival.

> **Corrección (verificada en log de partida real, 21-09):** escribí antes que los taps de 90ms
> impedían completar cargas reales y que *"el bot nunca ejecuta Power/Stun"*. **Es falso.**
> El stage `BOTLITE_COMBO_FINISH` mantiene `ALT_ATTACK` durante **650ms**, y `HOLD_SEQUENCE`
> lo re-presiona cada frame — 650ms > el umbral de 550ms, así que **el bot sí conecta Power
> melee hoy**, incluso en skill 1. El log muestra `SNAP self melee=POWER` y `chg=750`.
>
> También aparece `melee=SPEED_BREAKER`: soltar `ATTACK` antes de los 1000ms del Stun dispara
> un Speed Breaker (`bg_pmove.c:2492-2498`). O sea que el bot ya hace Breakers **por accidente**,
> sin decidirlos.
>
> Consecuencia para el plan: T2.1 no es *"habilitar"* las cargas — ya ocurren. Es **convertirlas
> en una decisión deliberada** en vez de un subproducto de los tiempos del combo. Y la tabla de
> capacidades está mal: skill 1 hoy hace Power melee y Speed Breakers.

Para skill 1 eso último se mantiene a propósito: **solo Speed taps**. Lo que cambia es que
reacciona a UNA señal: si `targetMeleeState` indica carga (Power/Stun), con probabilidad
gobernada por `blockTendency` del archetype → Block; si no, sigue presionando.

**Criterio de aceptación:** el bot bloquea de forma observable *cuando* el jugador carga, no
en momentos aleatorios. ~~Sigue sin lanzar Power/Stun nunca.~~ (ver corrección arriba: ya los
lanzaba antes de T1.1; separarlo es trabajo de T2.1)
**Riesgo:** medio — toca el núcleo del melee actual.

**Estado:** implementado como capa **previa** a la máquina de estados de combate, en
`BotLite_RunReactiveBlock()` (`g_botlite_melee.c`). Dispara sobre `snapshot->targetMeleeState`
∈ {`stMeleeChargingPower`, `stMeleeChargingStun`, `stMeleeStartPower`}.

Decisiones de diseño:
- **Una sola tirada por carga del rival** (`reactiveChargeSeen`). Evaluarla cada frame haría
  que el bot bloqueara prácticamente siempre — tan artificial como el timer que reemplaza.
- **Retardo de reacción** por skill, vía nuevas claves `reactive_block_min_ms` /
  `reactive_block_max_ms`: skill 1 380-750ms, skill 2 260-560ms, skill 3 170-400ms.
- Probabilidad = `blockTendency` del archetype (aggressive 0.25 … tactical 0.62).
- Si la carga termina antes de que llegue la reacción, la ventana se pierde: es lo que hace
  que un retardo alto se sienta como un bot más lento, no solo como un bot que bloquea menos.

Nuevo flag de policy `uses_reactive_block` (separado del `uses_block` por temporizador, que
sigue existiendo solo para skill 3 y se reemplazará en T3.1).

---

### T1.2 — Prohibir transformación en skill 1 ✅ HECHO
**Archivos:** `Game/Game/g_botlite_skill.c`, `Build-Remastered/ZEQ2/botsys/skills/skill1.cfg`

`BotLite_RunInitialTransform` (`g_botlite_skill.c:102-123`) hace un salto único tier 0→1 al
entrar en combate. Para skill 1: desactivarlo por completo vía `uses_transform=0` en
`skill1.cfg` + guard en la policy, de modo que nunca presione `BUTTON_POWERLEVEL` con fines
de transformación.

**Criterio de aceptación:** un bot skill 1 pelea toda la partida en tier base; verificable
en el HUD y en el log de debug.
**Riesgo:** bajo.

**Estado:** `skill1.cfg` ya traía `uses_transform=0`, que mapea a `policy->needsInitialTransform`
y bloquea la llamada en `g_botlite_skill.c:273`. Lo que faltaba era robustez:

- La prohibición se movió **dentro** de `BotLite_RunInitialTransform`, no solo en el call site.
  Un camino nuevo que llame ahí ya no puede transformar a un skill que tiene el flag en 0.
  Loguea `Transform not allowed for this skill`.
- **Agujero latente tapado:** `POWERLEVEL` + `forwardmove>0` es "subir de tier"
  (`bg_pmove.c:694`), y la acción del bot se **acumula durante el frame**. La carga de ki de
  skill 3 (`g_botlite_movement.c`) presionaba `POWERLEVEL` sin limpiar `forwardmove`, así que
  cualquier movimiento hacia adelante seteado antes en ese frame la habría convertido en una
  transformación involuntaria. Ahora se limpia explícitamente.

---

### T1.3 — Gestión mínima de stamina ✅ HECHO
**Depende de:** T0.3, T1.1
**Archivos:** `Game/Game/g_botlite_movement.c`, `g_botlite_resource.c`

Skill 1 no planifica stamina, solo evita suicidarse con ella: en `CRÍTICO`/`AGOTADO` deja de
boostear (`BotLite_ShouldUseBoost`, `g_botlite_movement.c:137`) y no intenta zanzoken.
Además, retirar el hack de "curación" con break-limit salvo emergencia real de vida baja.

**Criterio de aceptación:** con `g_botStamina 1`, un bot skill 1 nunca llega a fatiga negativa.
**Riesgo:** bajo.

**Estado:** el gateo de boost (CHEAP) y zanzoken (EXPENSIVE) ya había entrado con T0.3. Acá se
cerraron los dos gastos grandes que faltaban, que eran los que realmente podían vaciar la barra:

- **Remate de combo → Power melee.** Mantiene `ALT_ATTACK` 650ms, supera el umbral de 550ms y
  cobra `plMaximum * 0.05` de fatiga (`bg_pmove.c:2437`). Sin presupuesto `NORMAL` se degrada a
  un golpe de 90ms: no llega a cargar, así que no cobra.
- **Ataques de ki.** `BOTLITE_SKILL2_PRESSURE_START_ATTACK` ahora requiere presupuesto `NORMAL`.
  Una carga **ya empezada sí se libera** — retenerla desperdiciaría la energía invertida y
  dejaría al bot congelado con el arma cargada.

El hack de "curación" con break-limit no hizo falta tocarlo: está gateado a `info->skill == 3`.

---

# FASE 2 — Skill 2: Táctico

Todo lo de skill 1, más el uso real del sistema de combate.

### T2.1 — Cargas de Power y Stun deliberadas ✅ HECHO
**Depende de:** T1.1
**Archivos:** `Game/Game/g_botlite_melee.c`

Implementar mantenimiento sostenido del botón: `BUTTON_ALT_ATTACK` ~550ms para Power,
`BUTTON_ATTACK` ~1000ms para Stun (umbrales en `bg_pmove.c:2432` y `:2492`). Alternar con
Speed taps para no ser predecible. La decisión de comprometerse a una carga consulta el
presupuesto de stamina (T0.3) porque Power cuesta `plMaximum * 0.05` de fatiga
(`bg_pmove.c:2437`).

**Criterio de aceptación:** el bot conecta Power melee con knockback visible; el evento
`EV_MELEE_KNOCKBACK` aparece en el log.
**Riesgo:** medio-alto — es el cambio más profundo del melee.

### Hallazgo: hay dos rutas a Power melee **instantáneo**

Al leer `PM_Melee` a fondo (`bg_pmove.c:2547-2562`) aparecieron dos caminos que otorgan
`meleeCharge = 750` **de golpe** al iniciar la secuencia con `forwardmove > 0` y sin botones de
ataque — sin cargar nada:

| Ruta | Condición | Efecto extra |
|---|---|---|
| **A** | el rival está en knockback (`timers[tmKnockback] != 0`) | Power gratis como castigo |
| **B** | el bot viene boosteando **sostenido >2500ms** (`timers[tmBoost] > 2500`) | además deja al rival en `stMeleeStartHit` |

Eso explica el `chg=750` que apareció en el log de partida: no venía de mantener el botón,
venía regalado por una de estas dos rutas.

> **Trampa de la ruta B:** se rompe sola si el bot presiona ataque mientras se acerca.
> `BotLite_SanitizeButtons` descarta `BOOST` cuando hay `ATTACK`/`ALT_ATTACK`, `PM_CheckBoost`
> llama a `PM_StopBoost` y `tmBoost` vuelve a 0. Aproximarse *sin atacar* no es pasividad:
> es lo que carga el opener.

### Estado

- **El Power dejó de salir por accidente.** El remate del combo mantenía `ALT_ATTACK` 650ms
  (> umbral de 550) sin que nadie lo decidiera; ahora es un golpe de 90ms.
- **`BotLite_RunDeliberateCharge()`** es la única vía a Power/Stun. Se resuelve después del
  bloqueo reactivo (defensa primero) y antes del combo. Decide leyendo `targetMeleeState`:
  - rival cargando → **no** se compromete (su ataque sale antes; eso es del Breaker, T2.2)
  - rival en `EVADE` → no se compromete (Evade anula Power y Stun por igual)
  - rival vulnerable (`START_HIT`, `IDLE`, `START_DODGE`, knockback) → probabilidad alta,
    escalada por `aggression`
  - rival **bloqueando** → elige **Stun**: el Power pasa al 30% de daño contra block
    (`bg_pmove.c:2447`), el Stun no depende de eso
- **Skill 1 ya no conecta Power melee** — `uses_charge_attacks=0`, así que la tabla de
  capacidades vuelve a ser cierta.
- Cooldown (`charge_cooldown_ms`, 2600 por defecto) y presupuesto de stamina `NORMAL`.
- La carga **se aborta si el rival sale del melee**: sostener el botón sin nadie enfrente solo
  congelaría al bot.

---

### T2.2 — Breakers reactivos ✅ HECHO
**Depende de:** T2.1
**Archivos:** `Game/Game/g_botlite_melee.c`

> **Corrección grave: yo tenía la tabla invertida.** Verificado en `bg_pmove.c:2386-2428`.

| Breaker | Cómo se produce | Contra rival **cargando** | Falla contra |
|---|---|---|---|
| **Charge Breaker** (`tmMeleeBreaker = +1`) | tocar `ALT_ATTACK` y soltar antes de 550ms | ✅ **le pone `tmMeleeCharge = 0`** — le rompe la carga, le hace daño, lo congela 500ms, y al que lo ejecuta le suma `healthPool` y `maximumPool` | rebota contra `stMeleeUsingSpeed` (bot congelado 950ms); nulo contra Evade |
| **Speed Breaker** (`tmMeleeBreaker = -1`) | tocar `ATTACK` y soltar antes de 1000ms | ❌ **BACKFIRE CATASTRÓFICO: le regala `tmMeleeCharge = 1000`**, o sea una carga completa gratis | Block |

O sea que el único breaker válido contra una carga es el **Charge Breaker**, y el Speed Breaker
es lo peor que se puede hacer en esa situación.

**Consecuencia que había pasado desapercibida:** el stage PUNCH del combo toca `ATTACK` 90ms y
lo suelta — eso *es* un Speed Breaker. Cada vez que el jugador cargaba y el bot hacía PUNCH,
**el bot le estaba regalando una carga completa**. Por eso T2.2 incluye un guard que cambia el
PUNCH por `ALT_ATTACK` mientras el rival carga.

Prioridad implementada: **Charge Breaker → zanzoken defensivo → bloqueo → carga deliberada →
combo**. El breaker va primero porque rompe la carga en vez de amortiguarla, y encima da
recursos. Un intento por episodio de carga, cooldown de 650ms (`tmMeleeBreakerWait` impone
500ms propios).

**Criterio de aceptación:** el bot rompe cargas del jugador de forma consistente pero no
infalible; el backfire no se dispara más que ocasionalmente.
**Riesgo:** medio.

---

### T2.3 — Transformación multi-tier con sustain ✅ HECHO
**Depende de:** T0.3
**Archivos:** `Game/Game/g_botlite_skill.c`

> **Corrección (log de partida real, 21-09):** escribí que hoy era *"un único salto de tier
> 0→1"*. **Es falso.** `checkTier` (`g_tiers.c:62-85`) tiene un `while(1)` que encadena tiers
> mientras `keyTierUp` siga puesto y se cumplan los requisitos. Como el bot mantiene
> `BUTTON_POWERLEVEL` + forward varios frames, **sube de 0 al tier máximo del personaje de una
> sola vez**: el log muestra a Goku pasando de `tier=0` a `tier=4` y después
> `Transform skipped: no higher tier`.
>
> O sea que el bot **ya empieza cada combate en su forma final**, y pierde ~5 s sin poder pelear
> mientras corre la animación (`Waiting transform animation`). El trabajo de esta tarea no es
> *permitir* multi-tier — ya ocurre — sino **dosificarlo**: subir por etapas según la amenaza en
> vez de vaciar todo el repertorio en el primer segundo.

Reemplazar el encadenado automático a tier máximo por gestión real de tiers:
- subir mientras `requirement*` y **`sustain*`** lo permitan (`g_tiers.c:66-84`)
- **bajar voluntariamente** antes de que el sustain falle, para evitar la des-transformación
  forzada (`g_tiers.c:88-97`), que deja una ventana de vulnerabilidad
- re-transformarse si se cayó de tier

Con stamina real (T0.4) esto es crítico: `sustainFatigue` puede forzar la caída en pleno
combate.

**Criterio de aceptación:** el bot alcanza tiers > 1 en personajes que los tienen y no sufre
des-transformaciones forzadas repetidas.
**Riesgo:** medio.

---

### T2.4 — Carga de ki activa entre intercambios ✅ HECHO
**Depende de:** T0.3, T2.3
**Archivos:** `Game/Game/g_botlite_movement.c`, `g_botlite_tactics.c`

Hoy `BUTTON_POWERLEVEL` + derecha solo se usa como hack de curación en skill 3
(`g_botlite_movement.c:504`). Convertirlo en táctica legítima: cuando está lejos, sin línea
de vista, o el rival está en recovery, cargar ki/stamina. Debe **abortar al instante** si el
rival se acerca o ataca.

> **Bug encontrado al probar (21-09) y corregido: ki y stamina compiten, y no había arbitraje.**
>
> La primera versión cargaba ki siempre que estuviera por debajo del 85%. Eso creó un bucle:
> `statScale = 1 - (ki/kiMax)`, así que **mantener el ki alto fija la recuperación de fatiga en
> su mínimo (0.25, 3x más lenta)**, y encima `usingAlter` la anula del todo mientras se carga.
> El bot quedaba clavado en TIGHT/CRITICAL el **86% del tiempo** (87 + 31 lecturas contra 19
> COMFORTABLE), y como `BOTLITE_SPEND_EXPENSIVE` exige COMFORTABLE, **el zanzoken no se
> disparaba nunca** y el sanzoken de aproximación solo 8 veces en toda la sesión.
>
> Correcciones:
> - **Arbitraje explícito:** solo se carga ki con el presupuesto en `COMFORTABLE`. Cargar ki
>   con la stamina baja es autodestructivo, no un trade-off.
> - Umbral de ki bajado de 85% a 60%: cargar solo cuando de verdad falta.
> - El log era por frame (923 líneas de 1676): ahora registra solo el inicio de la ventana.

**Criterio de aceptación:** el bot carga ki en pausas naturales del combate y reacciona
inmediatamente si lo interrumpen.
**Riesgo:** medio — riesgo de que se quede cargando y sea daño gratis. La condición de aborto
es lo crítico de esta tarea.

---

### T2.5 — Zanzoken defensivo ✅ HECHO
**Depende de:** T0.3
**Archivos:** `Game/Game/g_botlite_melee.c`, `g_botlite_movement.c`

Hoy el zanzoken solo se usa como gap-closer ofensivo (`BotLite_UseApproachSanzoken`).
Agregar el uso defensivo: si detecta una carga de Power/Stun que no llega a bloquear a
tiempo, teletransportarse. El zanzoken **cancela el melee al instante** (`bg_pmove.c:2326-2329`),
así que es el escape definitivo. Consultar presupuesto de stamina: cuesta
`plMaximum * 0.12` (`:331`) y se bloquea con fatiga ≤ 1 (`:296`).

> **Ajuste tras la prueba:** pedía presupuesto `EXPENSIVE`, que exige `COMFORTABLE`, y en la
> práctica no se disparaba nunca. Pasó a `NORMAL`: evitar un Power melee ya comprometido es
> justamente el gasto "decisivo" que `TIGHT` habilita. El **sanzoken de aproximación**, que sí
> es opcional, se queda en `EXPENSIVE`.
>
> Nota de diseño: para skill 2/3 el zanzoken defensivo es deliberadamente **raro**, porque el
> Charge Breaker corre antes y es mejor opción (rompe la carga en vez de huir de ella). El
> zanzoken es el respaldo para cuando el breaker está en cooldown.

**Segundo ajuste (misma sesión):** el usuario señaló dos problemas de fondo, no de tuning —
ver la corrección completa en T2.8, porque terminaron siendo el mismo rediseño.

**Tercer bug, más grave, encontrado el 21-09 en una prueba posterior: el zanzoken defensivo
era código muerto.** El bot lo intentaba (17 líneas de log "Defensive zanzoken" en la sesión
anterior) pero **nunca ejecutaba de verdad** — el jugador jamás vio un teletransporte.

Causa, verificada con tres citas del motor:
- `usingZanzoken` solo se escribe en un lugar: dentro de `PM_CheckZanzoken`
  (`bg_pmove.c:344`), que **se niega por completo mientras `usingMelee` esté puesto**
  (`:316` — `if(... || usingMelee){return;}`).
- `usingMelee` se fija **una sola vez** al iniciar el intercambio (`PM_SyncMelee`, `:2532`) y
  queda puesto para **todo** el intercambio, hasta que se rompe distancia.
- El orden de llamada por frame es `PM_CheckZanzoken()` antes que `PM_Melee()`
  (`:3200` vs `:3227`) — ni siquiera hay una ventana de un frame útil una vez que el rival ya
  está cargando.

Mi condición original (disparar `BotLite_RunDefensiveZanzoken` solo si `snapshot->botInMelee`)
exigía exactamente el estado que garantiza que el motor lo descarte. El botón se presionaba, el
motor lo ignoraba en silencio, y el log registraba la *intención* del bot, no una ejecución real.

**Corrección (2da):** la función se movió del chain de melee (donde es estructuralmente
inalcanzable) a `BotLite_RecoverRunFlee` (T2.8), el único punto donde `!botInMelee` estaba
garantizado por construcción.

**CUARTO bug — la 2da corrección tampoco funcionó, y el error fue más sutil.** En la
siguiente partida el usuario reportó de nuevo "no usa zanzoken en ningún momento". El log lo
confirmó: **0 ejecuciones**.

Causa: moví la llamada al único lugar donde `!botInMelee` estaba garantizado, pero **nunca
verifiqué que ese lugar fuera alcanzable**. `BotLite_RecoverRunFlee` solo corre dentro del
modo `RECOVER`, que exige stamina ≤ 28%. En partida real **la fatiga del bot nunca bajó de
35%** (mínimo medido en el log), así que `RECOVER` jamás se activó y la función quedó como
código muerto por segunda vez, por una razón distinta.

La lección: acoplé una capacidad a un estado que casi nunca ocurre. Resolver "¿dónde es
legal?" no alcanza — también hay que preguntarse "¿con qué frecuencia se llega ahí?".

**Corrección (3ra, la que quedó):** el zanzoken defensivo se evalúa de forma **independiente
desde el think loop**, en cualquier momento en que el bot esté fuera de melee y realmente
amenazado. Sigue respetando la restricción dura del motor, pero ya no depende de ningún
umbral de recursos.

Qué cuenta como "amenazado" fuera de melee (cualquiera de los tres):
- viene de un knockback y el rival se le está acercando
- el rival está cargando algo y el bot está en su rango corto
- está en desventaja clara de vida con el rival encima

Un simple "el rival está cerca" **no alcanza**: durante una aproximación normal el bot
*quiere* cerrar distancia, y teletransportarse sería contraproducente.

**Criterio de aceptación:** el bot escapa de cargas comprometidas del jugador de forma
observable, sin agotarse la stamina en el proceso.
**Riesgo:** medio.

---

### T2.6 — Ball-flip: escape del knockback propio ✅ HECHO
**Depende de:** T0.3
**Archivos:** `Game/Game/g_botlite_ai.c`

Hoy solo skill 3 lo hace (`g_botlite_ai.c:90-98`, vía `skill3KnockbackCancelDelay`).
Extenderlo a skill 2 con delay mayor (más lento en reaccionar), dejando skill 1 sin esta
capacidad. Recordar que la ventana de cancelación requiere `tmKnockback < 4000`
(`bg_pmove.c:247`) y que cuesta fatiga proporcional (`:248`).

**Criterio de aceptación:** skill 2 se recupera del knockback más lento que skill 3; skill 1
no se recupera.
**Riesgo:** bajo.

---

### T2.7 — Decisión de desenganche ✅ HECHO
**Depende de:** T0.3, T0.4 (flipeado)
**Archivos:** `Game/Game/g_botlite.h`, `g_botlite_ai.c`, `g_botlite_resource.c`

Hoy los modos son `IDLE / SEARCH / COMBAT / WAIT_TARGET_RECOVERY`: **no existe el concepto de
retirarse a recuperarse**. Un bot sin stamina sigue peleando hasta morir.

Agregar `BOTLITE_MODE_RECOVER` + `BOTLITE_GOAL_RECOVER`, y una decisión periódica de tres vías
cuando el presupuesto (T0.3) cae a `CRÍTICO`/`AGOTADO`:

| Decisión | Cuándo | Qué hace |
|---|---|---|
| **Seguir peleando** | ventaja clara (rival más agotado / a punto de morir) | ignora el déficit y remata |
| **Defensa mínima** | rival encima, sin espacio para huir | Block/Evade, movimiento mínimo, recupera lo que pueda sin desengancharse |
| **Huir y recuperar** | hay distancia o LOS rota | rompe el lock, se aleja, entra en el ciclo de T2.8 |

La decisión debe tener **histéresis**: umbrales distintos para entrar y salir de `RECOVER`,
si no el bot oscila entre pelear y huir cada frame. Y debe abortar de inmediato si el rival
se acerca (reusar la condición de aborto de T2.4).

> **Bug encontrado al probar (21-09 noche): el modo RECOVER nunca se activaba.** El usuario
> reportó "tampoco considera huir en ningún momento" y el log lo confirmó: 0 activaciones.
>
> Causa: implementé el disparador **solo por stamina**, cuando la propia especificación de
> T3.6 decía *"stamina en CRÍTICO/AGOTADO, **vida baja**, o ki insuficiente"*. El disparador
> por vida nunca se escribió. Y como la stamina del bot se mantenía sana (mínimo medido: 35%,
> contra un umbral de 28%), **el bot recibía daño hasta morir sin retirarse jamás** — moría
> con la stamina intacta.
>
> **Corrección:** dos disparadores independientes, stamina **o** vida. Se agregó
> `BotLite_HealthPercent()` usando `plMaximum` como referencia de vida llena — verificado
> en `bg_pmove.c:678`, donde `plHealth` regenera hacia `plMaximum`. La salida exige que
> **ambos** recursos estén por encima de sus umbrales de salida, para no volver al combate
> curado pero exhausto ni al revés. Nuevas claves: `recover_health_enter_pct` (40) y
> `recover_health_exit_pct` (70).

**Criterio de aceptación:** un bot exhausto **o herido** se retira de forma legible en vez de
morir peleando; no oscila; vuelve al combate con recursos utilizables.
**Riesgo:** medio-alto — es un modo nuevo en la máquina de estados, toca el ciclo principal.

---

### T2.8 — Ciclo de recuperación: huir → bajar ki de una → descansar ✅ HECHO (rediseñado)
**Depende de:** T2.7
**Archivos:** `Game/Game/g_botlite_resource.c`, `g_botlite.h`, `g_botlite_main.c`

> **Rediseño completo tras feedback de partida real (21-09).** La primera versión (pulsos
> cortos de 180ms con descanso de 520ms) tenía dos problemas de fondo que el usuario señaló:
>
> 1. **Bajaba ki en pleno combate.** `BotLite_RunRecoverCycle` no comprobaba si el bot seguía
>    enganchado — drenaba ki aunque el rival estuviera encima. Bajar ki no sirve de nada si de
>    todos modos van a seguir pegándole; primero hay que soltarse.
> 2. **El pulsado era innecesariamente lento.** Investigando la razón de ser del pulso encontré
>    que estaba resolviendo un problema que no existe: la rama de bajada de ki
>    (`bg_pmove.c:709`, `rightmove<0`) corre en **cada tick de física**, fuera del bloque de
>    100ms que rige la regeneración pasiva. No hay ningún throttle del motor que pulsar. El
>    diseño pulsado descansaba el 74% del tiempo sin necesidad — sostener el botón es más rápido
>    y el costo (que `usingAlter` bloquee el `recovery` mientras dura) se paga una sola vez en
>    vez de repetirse en cada pulso.

**Diseño nuevo, tres etapas secuenciales (no simultáneas):**

1. **DISENGAGE** — huir hasta romper el contacto: fuera de melee y a más de 900 unidades, o sin
   línea de vista. Mientras el rival esté encima, el bot corre en dirección opuesta a él
   (yaw recalculado cada frame) y **no toca el ki en absoluto**.
2. **DRAIN** — ya lejos, bajar el ki de un solo tirón hasta el piso (22%), **sosteniendo** el
   botón en vez de pulsarlo.
3. **REST** — soltar el botón por completo y quedarse quieto. Con el ki en el piso `statScale`
   queda en su techo (0.75, hasta 3x más rápido que con ki alto) y estar parado da el `idleScale`
   ×2.8 (`bg_pmove.c:638`). Es el único momento en que la fatiga sube.

**Si el rival vuelve a cerrar distancia en cualquier etapa** (`BotLite_RecoverIsDisengaged`
pasa a falso), se vuelve a DISENGAGE de inmediato — recuperarse en pleno combate ya no es
posible por diseño, no solo por probabilidad.

La fase de recarga de ki que describía originalmente esta tarea no hizo falta agregarla aparte:
una vez que `BotLite_UpdateRecoverMode` sale de `RECOVER` (stamina por encima del umbral de
salida), el flujo normal retoma y `BotLite_RunKiCharge` (T2.4) recarga el ki oportunistamente en
la siguiente pausa segura — ya tiene su propio gateo a presupuesto `COMFORTABLE`.

**Criterio de aceptación:** el bot no baja ki mientras `botInMelee` es verdadero; el log muestra
las tres transiciones (`disengaging again` / `disengaged, draining ki` / `ki at floor, resting`)
en orden; el ciclo completo (huir + drenar) tarda segundos, no las decenas de segundos que
tomaba el pulsado.
**Riesgo:** medio. El punto a vigilar es que DISENGAGE no se cuelgue si el rival es más rápido
que el bot — hoy no tiene salida por tiempo propia, solo el timeout general de `RECOVER`
(`BOTLITE_RECOVER_MAX_MS`).

---

# FASE 3 — Skill 3: Experto

Todo lo de skill 2, más explotación activa del sistema contra el jugador.

### T3.1 — Evade como opción real ✅ HECHO
**Depende de:** T2.2
**Archivos:** `Game/Game/g_botlite_melee.c`, `g_botlite.h`, `g_botlite_profile.c`, `g_botlite_main.c`

**Tabla de contras verificada línea por línea contra `bg_pmove.c`** (no asumida — la mecánica
de Evade nunca se había leído a fondo antes de esta tarea):

| | Speed hit | Power hit | Charge Breaker | Speed Breaker |
|---|---|---|---|---|
| **Block** | reduce a 20% dmg | reduce a 30% dmg | **NO lo detiene** | **SI lo detiene** |
| **Evade** | anula, atacante paga fatiga extra | anula, evade paga 0.4x (barato) | **anula GRATIS** (`:2404`) | **NO lo detiene** (`:2417`, sin gate) |

Evade es estrictamente mejor que Block contra Speed/Power/Charge-Breaker — pero un rival que
lo note puede castigarlo con Speed Breaker, que Evade no detiene y Block sí. Por eso **no
conviene Evadir siempre**: eso volvería al bot mismo predecible y explotable.

**Estado:** al comprometerse a defender, el bot sortea entre Block y Evade (T3.2 sesga esa
probabilidad). Evade no usa botón — es `forwardmove < 0` mientras sigue encerrado en melee
(`bg_pmove.c:2606`), así que no compite con `SanitizeButtons` por otros botones.

**Criterio de aceptación:** el bot no responde siempre igual a la misma situación; la
distribución de respuestas es medible en el log (`Reactive defense: ... -> EVADE` vs `BLOCK`).
**Riesgo:** medio.

---

### T3.2 — Lectura de patrón del rival ✅ HECHO
**Depende de:** T3.1
**Archivos:** `Game/Game/g_botlite.h`, `g_botlite_melee.c`, `g_botlite_main.c`

> **Replanteo tras la corrección de T2.2/T3.1:** el plan original era "detectar si el rival
> abusa de Power o Stun y sesgar qué Breaker usar". Ya no aplica — Charge Breaker es el
> **único** contra válido contra una carga (Power o Stun, sin distinción); no hay "qué breaker
> elegir" que leer. Lo que sí vale la pena leer, dada la tabla de T3.1: un rival que abusa de
> **Speed Breaker** explota exactamente la debilidad de Evade. Si el bot lo nota, conviene
> sesgar hacia Block.

**Estado:** un EMA (media móvil exponencial) en vez de un buffer circular de N=8 — más simple,
mismo resultado. Cada vez que se ve al rival **entrar** en `stMeleeUsingSpeedBreaker` (una vez
por ocurrencia, no por frame), el sesgo sube; decae de a poco en cada intercambio. Prior
neutro de 0.3 (no 0, para no ser 100% Evade desde el arranque — eso también sería un patrón
fijo). Persiste entre exchanges dentro de la misma vida (vive en `runtime`, no en `melee` que
se limpia en cada transición); se reinicia al cambiar de objetivo o al morir.

**Criterio de aceptación:** contra un rival que repite Speed Breaker, el bot bloquea cada vez
más seguido en vez de Evadir; contra uno que no lo usa, se mantiene mayormente Evade. Visible
en el log: `bias=NN%` en cada línea `Reactive defense`.
**Riesgo:** medio — es la tarea más nueva conceptualmente.

---

### T3.3 — Punish a distancia tras knockback ⭐ ✅ HECHO
**Depende de:** T0.1, T2.1
**Archivos:** `Game/Game/g_botlite_skill.c`, `g_botlite.h`, `g_botlite_main.c`

Cuando el bot conecta un Power melee, el rival recibe `tmKnockback = 5000` y sale volando,
mientras el melee del bot **se corta automáticamente** (`bg_pmove.c:2461-2462`).

**Restricción de diseño clave — la ventana es corta y parcialmente disputada:**
- `tmKnockback` 5000 → 4000: **~1000ms garantizados**, el rival no puede hacer nada
- por debajo de 4000: el rival puede cancelar con ball-flip (`bg_pmove.c:247`)

**Estado:** `BotLite_UpdateKnockbackPunish` compara el costo real de carga del arma elegida
(`costs_chargeTime * costs_chargeReady`, de `g_userWeapon_t`) contra el margen que queda antes
de que `tmKnockback` cruce el piso de 4000, con 300ms de colchón para vuelo de proyectil y
reacción. Si no entra en el margen, no se fuerza — el bot persigue normalmente. Decisión única
por episodio (se resetea cuando `targetKnockbackTime` vuelve a 0). Probabilidad final: 40-80%
según `rangedBias` del archetype. Gateado a presupuesto `NORMAL` y a `info->skill == 3`
directamente (mismo patrón que el hack de curación pre-existente).

**Criterio de aceptación:** tras un Power melee conectado, el bot a veces castiga a distancia
y a veces persigue — no siempre lo mismo. El ataque elegido alcanza a salir dentro de la
ventana (verificable: `chargeMs <= windowMs` en el log `Knockback punish: ...`).
**Riesgo:** medio.

---

### T3.4 — Break-limit ofensivo ✅ HECHO
**Depende de:** T2.4, T0.4
**Archivos:** `Game/Game/g_botlite_resource.c`, `g_botlite.h`, `g_botlite_ai.c`, `g_botlite_profile.c`

> **Corrección de expectativa:** pushear ki más allá del 100% no aumenta el daño del *siguiente*
> ataque — el daño de un arma escala con la carga de **esa arma** (0-100% propio), no con el ki
> global. Lo que hace `isBreakingLimit` (`bg_pmove.c:722-744`) es convertir el exceso de ki en
> `healthPool`/`maximumPool`: vida y ki máximo casi permanentes para el resto del combate. La
> tarea no es "cargar antes de un golpe fuerte", es "explotar una ventana segura para hacer
> crecer los recursos".

**Estado:** se dispara cuando el rival no puede castigar (crasheado, en knockback, o
recuperándose — reutiliza las señales de T3.3), con ki ya alto (≥85%, si está bajo primero hay
que cargar lo normal vía T2.4) y presupuesto `COMFORTABLE` (romper el límite anula su propia
recuperación de fatiga — `bg_pmove.c:647` — así que solo tiene sentido si ya viene bien
parado). Ventana de 550ms, decisión única por episodio de vulnerabilidad. Corre **antes** que
T2.4 en el think loop por ser más específico.

**Criterio de aceptación:** el bot entra en break-limit en momentos de ventaja (rival
vulnerable), no cuando está siendo presionado. Log: `Offensive break-limit: ki=NN% window=550ms`.
**Riesgo:** medio-alto — depende de que T0.4 esté estable.

---

### T3.5 — Economía vida / ki / stamina ✅ HECHO
**Depende de:** T0.4, T3.4
**Archivos:** `Game/Game/g_botlite_ranged.c`

**Estado (alcance acotado a lo que pide el criterio de aceptación):** el gancho ya existía —
`BotLite_PickSkill2Weapon` elige al azar entre los slots de arma habilitados cada vez que el
bot inicia un ataque nuevo. Se sesgó esa elección: con `costs_health > 0`
(`g_userweapons.h`) en algún slot, y el bot **detrás en vida** respecto al rival
(`ps.lockedPlayer->powerLevel[plHealth]`, directo, sin pasar por el snapshot), esos slots se
excluyen del sorteo — salvo que dejar solo eso vacíe el pool, en cuyo caso se usa lo que haya
(jugar conservador no significa negarse a pelear).

La gestión de ki/stamina ya tiene tareas dedicadas (T2.4/T2.7/T2.8/T3.4); no se re-litigó acá
para no duplicar trabajo.

**Criterio de aceptación:** con ventaja de vida, el bot acepta armas costosas en vida (quedan
en el pool); en desventaja, las evita mientras haya alternativa.
**Riesgo:** medio.

---

### T3.6 — Ocultamiento defensivo por ki mínimo ✅ HECHO
**Depende de:** T2.7, T2.8
**Archivos:** `Game/Game/g_botlite_resource.c`

**Estado: en gran parte ya emergía de T2.7/T2.8**, que construyen exactamente el ciclo
huir→bajar ki→descansar que esta tarea pedía. Lo que agregó T3.6 específicamente:

- **Piso de ki más profundo para skill 3.** El piso general de T2.8 (22%) resultó insuficiente
  para "casi invisible": el alfa del blip en el radar del rival es literalmente
  `plCurrent/plMaximum` (`cg_radar.c`), así que 22% sigue siendo bastante visible, solo tenue.
  Skill 3 baja hasta 4% en la etapa DRAIN. Es seguro porque el zanzoken depende de **fatiga**,
  no de ki (`bg_pmove.c:296`) — bajar el ki no lo bloquea, solo lo hace más lento, aceptable
  para un bot que en ese momento evita pelear.
- **Re-enganche por decisión propia, buscando al jugador más cercano** — el pedido explícito
  del usuario. Al salir de `RECOVER` por stamina recuperada o por timeout, el modo pasa a
  `BOTLITE_MODE_SEARCH` (no vuelve directo a `COMBAT`), lo que hace que
  `BotLite_FindNearestVisiblePlayer` reevalúe desde cero — el bot puede terminar peleando con
  el mismo rival si sigue siendo el más cercano, o con otro si no. La única salida que sí
  vuelve al mismo objetivo es la de "apareció una ventana" (`BotLite_RecoverHasAdvantage`),
  porque esa ventana es específica de ese rival.

Restricciones de diseño que ya venían de T2.7 y se mantienen intactas: sin radar propio (el
bot no puede cazar a un jugador escondido, solo esconderse él — `g_radar.c` excluye
`SVF_BOT`), techo de tiempo (`BOTLITE_RECOVER_MAX_MS`), histéresis entrada/salida, y disparo
exclusivo por déficit real de recursos — nunca con vida alta ni en ventaja.

**Criterio de aceptación:** el blip del bot se desvanece de forma observable en el radar del
jugador (ki ≤4%); el bot reaparece tras un tiempo acotado y sale a buscar al jugador más
cercano por su cuenta; nunca se esconde estando en ventaja ni con vida alta.
**Riesgo:** medio — el riesgo real es de *diseño* (que resulte frustrante), no de código.
Validar con partidas reales.

---

# FASE 4 — Tuning y cierre

### T4.1 — Exponer los nuevos tunables en botsys ✅ HECHO
**Depende de:** Fases 1-3
**Archivos:** `Game/Game/g_botlite.h`, `g_botlite_profile.c`, `g_botlite_resource.c`,
`g_botlite_struggle.c`, `g_botlite_melee.c`, `Build-Remastered/ZEQ2/botsys/skills/*.cfg`,
READMEs (`README_EN.txt` / `README_ES.txt`)

> **Alcance real vs. lo planeado:** el ciclo de recuperación dejó de ser "pulso + pausa"
> durante la corrección de T2.8 (ver esa tarea) — ya no hay pulso ni fase de curación
> separada que tunear, así que esos ítems del plan original no aplican. El resto sí se
> movió tal como estaba previsto.

**16 tunables** movidos de `#define` fijos en C a campos de `botlite_profile_t`, leídos
desde `.cfg` (`Game/Game/g_botlite.h`, `g_botlite_profile.c`): umbrales de presupuesto de
stamina (COMFORTABLE/TIGHT/CRITICAL), distancia de escape con zanzoken, distancia y objetivo
de carga de ki, umbrales de entrada/salida/techo de tiempo/distancia segura/pisos de ki del
ciclo de recuperación (incluido el piso "oculto" de skill 3 agregado en T3.6), umbral y
ventana del break-limit ofensivo, umbral de compromiso al forcejeo de haces, y duración del
toque del Charge Breaker.

Quedaron **fuera** de este movimiento, deliberadamente:
- `BOTLITE_STRUGGLE_KI_FLOOR` (=1) — refleja una constante del **motor**
  (`bg_pmove.c:705`), no una preferencia de diseño; cambiarla desalinearía el código del
  valor real que usa `PM_UsePowerLevel`.
- `BOTLITE_CHARGE_NONE/POWER/STUN` — son IDs internos de una máquina de estados, no
  números que un diseñador querría ajustar.

Todos los defaults en C quedaron **idénticos** a los `#define` que reemplazan, y además
espejados en `botsys/skills/defaults.cfg` — así que este cambio no altera el comportamiento
por sí solo, solo lo vuelve editable sin recompilar. `struggle_commit_pct` es la única clave
de esta tanda que **sí** difiere por skill (75/40/20), sobreescrita en cada `skillN.cfg`.

**Ambos READMEs actualizados** — y no solo con lo de T4.1: estaban desactualizados desde
**T1.1**, sin documentar ninguna de las claves de bloqueo/evade reactivo, cargas
deliberadas, breakers, zanzoken defensivo, carga de ki, modo recover ni break-limit
ofensivo agregadas a lo largo de toda la Fase 1-3. Se completó esa documentación entera,
no solo el incremento de esta tarea.

Verificado: `skill1.cfg` mantiene `uses_transform=0` (T1.2).

> **Ampliación posterior (21-09 noche): la banda del sanzoken de aproximación era
> prácticamente inalcanzable.** El usuario reportó no ver sanzoken nunca; el log mostró solo
> 6 usos en toda la sesión. La causa no era el presupuesto sino la geometría: la banda
> reusaba los umbrales híbridos ranged/melee, o sea **10000–12000 unidades** en skill 3.
> Midiendo las distancias reales de combate en el log: mediana 2019, p25 **58** (melee),
> p75 **13415**. El bot está casi siempre o encima del rival o lejos — esa franja de 2000
> unidades la cruza de paso mientras cierra distancia.
>
> Se le dieron claves propias, desacopladas de los umbrales híbridos:
> `sanzoken_band_min_dist` (1500) y `sanzoken_band_max_dist` (16000), cubriendo el rango que
> el bot efectivamente recorre. Con ambas en 0 vuelve al comportamiento anterior.
>
> También se agregaron las claves de retirada por vida (`recover_health_enter_pct` /
> `recover_health_exit_pct`) y la de Fase 5 (`uses_dodge_incoming`).

**Riesgo:** bajo.

---

### T4.2 — Matriz de validación
**Depende de:** todo

Escenarios a probar por cada skill, con `g_botStamina` en 0 y en 1:

1. Bot vs. jugador pasivo → ¿presiona sin agotarse?
2. Bot vs. jugador que spamea Power → ¿aparece el Breaker? (skill 2+)
3. Bot vs. jugador que spamea el mismo ataque → ¿mejora la respuesta? (skill 3)
4. Bot atrapado en struggle → ¿escapa? ¿escala con el skill?
5. Bot en knockback → ¿ball-flip según skill?
6. Bot que conecta Power → ¿punish o persecución? (skill 3)
7. Combate largo (3+ min) con stamina real → ¿se auto-mata por fatiga negativa?
8. Personaje multi-tier → ¿sube tiers sin des-transformaciones forzadas? (skill 2+)
9. Skill 1 → ¿nunca se transforma? ¿nunca lanza Power/Stun?
10. Bot vs. bot de distinto skill → ¿gana el de skill mayor de forma consistente?
11. Bot exhausto → ¿se retira en vez de morir peleando? ¿sin oscilar entre huir y pelear? (skill 2+)
12. Bot en ciclo de recuperación → ¿**pulsa** el ki en vez de sostenerlo? (`usingAlter` debe
    alternar; sostenido = recuperación cero) (skill 2+)
13. Bot que terminó de curarse → ¿queda con stamina utilizable, o sano pero exhausto? (skill 2+)
14. Bot escondido → ¿su blip se desvanece del radar del jugador? ¿reaparece dentro del techo
    de tiempo **y sale a buscar al jugador más cercano**? (skill 3)
15. Bot **en ventaja o con vida alta** → ¿nunca se esconde ni se retira sin necesidad? (skill 3)
16. Bot recuperado → ¿re-engancha por decisión propia, sin quedarse campeando? (skill 3)

---

# FASE 5 — Esquivar ataques entrantes ✅ HECHO

> Pedido del usuario: el bot no tenía **ninguna** percepción de proyectiles en vuelo — solo
> reaccionaba a lo que ya lo estaba tocando (`isStruggling`, daño recibido, knockback).
> Esta fase es sobre anticipar, no reaccionar.

**Archivos:** nuevo `Game/Game/g_botlite_dodge.c`, `g_botlite.h`, `g_botlite_profile.c`,
`g_botlite_ai.c`, `g_botlite_main.c`, `Makefile`, `game.bat`, `game.q3asm`,
`botsys/skills/*.cfg`

## Por qué se puede hacer sin trucar nada

El bot corre en el game module y ve los `gentity_t` **completos**, no la vista replicada de
un cliente. Los proyectiles son entidades normales con su física expuesta: `s.pos.trBase` /
`s.pos.trDelta` (trayectoria), `r.ownerNum` (quién disparó), `s.eFlags & EF_GUIDED`.
No hace falta darle información privilegiada — es la misma que cualquier observador tendría.

## T5.1 — Percepción ✅

`BotLite_FindIncomingThreat` recorre las entidades **desde `MAX_CLIENTS` en adelante** (los
primeros slots son jugadores; los proyectiles viven arriba) y filtra por dueño hostil.

La predicción es el **punto de máxima aproximación recta-punto, resuelto analíticamente**:
con el proyectil en `P` a velocidad `V` y el bot en `B`, la distancia² en el tiempo es una
parábola cuyo mínimo está en `t = -((P-B)·V)/|V|²`. Si `t ≤ 0` el proyectil ya pasó de largo
y se aleja — no es amenaza. Es geometría cerrada, no simulación paso a paso: barato por frame.

> **Límite documentado en el código — proyectiles guiados:** un `EF_GUIDED` corrige rumbo
> hacia su objetivo, así que extrapolar su `trDelta` en recta da la respuesta *equivocada*
> (parece que va a errar cuando en realidad va a doblar). Para esos **no se predice
> trayectoria**: se usa proximidad cruda, que es la única señal honesta sin duplicar la
> lógica de homing del motor. Se les asigna una urgencia proporcional a la cercanía para
> poder compararlos en la misma escala que las amenazas calculadas.

## T5.2 — Decisión de esquive ✅

Según el tiempo hasta el impacto:
- **< 350ms** → zanzoken (única reposición lo bastante rápida). Respeta la restricción dura
  del motor descubierta en T2.5: nada de zanzoken con `usingMelee` puesto.
- **resto de la ventana** → desplazamiento lateral perpendicular a la trayectoria, eligiendo
  el lado por producto cruz y **desempatando hacia donde el bot ya mira** — así el esquive no
  lo obliga a girar en redondo, que lo dejaría más expuesto que quedarse quieto.

Skill 1 no esquiva (`uses_dodge_incoming=0`); skill 2 y 3 sí.

## T5.3 — Aguantar y contraatacar ✅

No siempre conviene esquivar: con ventaja clara de vida (>2x), interrumpir la propia ofensiva
cuesta más de lo que ahorra. Reutiliza la misma noción de ventaja que T2.7 y T3.5 en vez de
inventar una nueva. Con la amenaza ya encima (<350ms) no se "decide" aguantar — ya no hay opción.

## Pendiente de validar en partida

El módulo compila y está cableado, pero **no está probado en juego todavía**. Ver la sección
conjunta al final del documento: *Pendiente de validar en partida (Fase 5 y Fase 6)*.

---


---

# Fase 6 — Alternar melee / ranged por decisión, no por distancia ✅ HECHO

## El problema, medido

`BotLite_UpdateHybridRangeMode` encendía `forceMelee` cuando `dist <= ranged_to_melee_distance`
y lo apagaba cuando `dist > melee_to_ranged_distance`. Los valores de los `.cfg` eran:

| skill | enter | exit |
|-------|-------|------|
| 2     | 10000 | 10000 (forzado a 10040 por el mínimo de histéresis) |
| 3     | 10000 | 12000 |

Contra las distancias reales medidas en el log de una sesión: **p25 = 58, mediana = 2019,
p75 = 13415**. O sea que en la práctica el bot estaba casi siempre por debajo de 10000 y el
modo ranged era inalcanzable salvo cuando el rival se iba al otro extremo del mapa.

No era un bot híbrido con un sesgo hacia el melee: era un bot de melee puro con una rama de
ranged que casi nunca se ejecutaba. **La distancia no era una entrada de la decisión, era la
decisión entera.**

## Diseño

Capa nueva en `g_botlite_engage.c`: el bot puntúa melee contra ranged y la distancia pasa a
ser **uno** de los términos. Score positivo = melee, negativo = ranged, normalizado por la suma
de pesos para que el margen signifique lo mismo en los tres skills.

### De dónde sale cada término

Todos verificados en el motor, no estimados:

| Término | Empuja a | Por qué |
|---------|----------|---------|
| ki bajo | melee | El daño de Power melee es `plCurrent * 0.15` (`bg_pmove.c:2460`) y el coste en fatiga de un ataque de ki escala con `energyScale = plCurrent / plMaximum` (`bg_pmove.c:2769`). Con ki bajo el ranged pega poco; el Speed melee sigue entero porque sale de la fatiga (`plFatigue * 0.013`, `bg_pmove.c:2583`). |
| stamina alta | melee | Un Power melee cuesta `plMaximum * 0.05` de fatiga y la rama del breaker `0.10` (`bg_pmove.c:2452` y `2544`). |
| vida baja | melee | El daño hecho se convierte en vida propia. Power melee convierte a **1.0 / 0.8** (`bg_pmove.c:2479-2480`), Speed melee a 0.7 / 0.5 (`2594-2595`), un ataque de ki a 0.7 / 0.3 (`g_usermissile.c:549-550`). **El melee es el mejor curador del juego.** Gated: sólo aplica con ki ≥ 35%, porque sin ki el Power no hace daño y entonces tampoco cura. |
| rival cargando ki | melee | Entrar en melee le fuerza `PM_WeaponRelease()` (`bg_pmove.c:2742`): meterse encima le corta la carga. |
| rival bloqueando | ranged | El bloqueo reduce Speed melee a 0.2x y Power a 0.3x (`bg_pmove.c:2588` y `2461`). |
| rival en knockback | ranged | Está volando: no hay a quién agarrar, pero sí a quién tirarle. |
| tier del rival mayor | ranged | No darle el duelo cuerpo a cuerpo. |
| distancia | ambos | Término suave. En skill 3 es de los que **menos** pesa (0.60 contra 1.30 del estado del rival). |
| efectividad | ambos | Lo único medido. Ver la advertencia abajo. |

### Anulaciones duras (no son preferencias, son límites del motor)

- **`botInMelee` → melee.** Dentro de un lock el arma no dispara: `PM_WeaponRelease()` y
  `return` incondicionales en `bg_pmove.c:2742`. Querer ranged ahí no significa nada.
- **Sin línea de vista → melee.** No hay tiro posible; hay que acercarse igual.

### Histéresis

Permanencia mínima (`engage_switch_min_ms`) más un margen de score (`engage_switch_margin`):
no alcanza con cruzar el cero, hay que cruzarlo con convicción. Sin el margen el bot se pasaría
la pelea cambiando de idea en el borde.

## Kiting (sólo skill 3)

Decidir ranged con el rival encima no sirve de nada si el bot no hace algo para conseguir la
distancia. `BotLite_RunRangedKite` retrocede de frente al rival hasta pasar
`engage_kite_min_dist`.

Lo que lo hace posible: el motor rompe el lock de melee cuando la distancia pasa de 64 unidades
(`bg_pmove.c:2362`) y con `forwardmove < 0` el estado del duelo pasa a `stMeleeDegressing`
(`bg_pmove.c:2356`). Retroceder es una salida legítima del melee — no hace falta zanzoken.

Es distinto de huir: huir lo resuelve el modo RECOVER (T2.7) y termina con el bot escondido.
Acá el bot mira al rival todo el tiempo; el hueco es para disparar.

## Efecto lateral que hubo que cubrir

`BotLite_RunRangedPressure` apunta y dispara pero **no mueve al bot**. Con la lógica vieja casi
no se notaba porque el modo ranged apenas se alcanzaba. Ahora que puede ser la mitad del
combate, un bot que no acorta se convierte en un francotirador plantado. Se agregó
`BotLite_ApproachWhileShooting`: avanza sólo por fuera de `engage_neutral_dist`, para no
meterse solo en el melee que justamente decidió evitar.

## Lo que es aproximado y por qué

El término de **efectividad** compara la vida del rival entre dos muestras. En un FFA el rival
puede estar recibiendo daño de un tercero y el bot se lo anota como propio. No hay una fuente
limpia de "daño que hice yo" accesible desde el módulo sin tocar `g_combat.c`. Se acota
exigiendo que siga siendo el mismo objetivo y descartando muestras separadas por más de 1000ms,
pero **sigue siendo una aproximación**. Por eso pesa 0.50 en skill 3 y arranca en **0** en
skill 1 y 2 — es el único término que se puede apagar sin perder nada estructural.

## Reparto por skill

| | skill 1 | skill 2 | skill 3 |
|---|---|---|---|
| `uses_engage_intent` | 0 | 1 | 1 |
| `uses_engage_kiting` | 0 | 0 | 1 |
| permanencia | — | 4000ms | 2200ms |
| margen | — | 0.35 | 0.22 |
| peso distancia | — | 0.90 | **0.60** |
| peso estado rival | — | 0.60 | **1.30** |
| peso efectividad | — | 0.00 | 0.50 |

Skill 1 queda con la lógica de distancia de siempre — consistente con que no se transforma ni
usa zanzoken defensivo. Skill 2 decide, pero lento y sin abrir hueco. Skill 3 es el único donde
el estado del rival pesa más que la distancia: ahí está el punto de toda la fase.

---

# Bugs de inicializadores posicionales encontrados de paso ✅ CORREGIDOS

Revisando `g_botlite_profile.c` para agregar los tunables nuevos aparecieron **dos** desalineos
en inicializadores posicionales. Los dos son míos, introducidos durante este backlog.

## 1. `botlite_profile_t` — 20 campos corridos dos lugares

`chargeCommitCooldown` y `stunChargeHoldMs` (agregados en T2.1) quedaron en el **medio** del
inicializador, justo después de `reactiveBlockMaxReaction`, pero declarados al **final** del
struct. Todo desde `staminaComfortablePct` en adelante tomaba el valor del vecino:

| campo | recibía | debía recibir |
|---|---|---|
| `staminaComfortablePct` | 2600 | 70 |
| `staminaTightPct` | 1050 | 30 |
| `recoverEnterPct` | 1200 | 28 |
| `recoverKiFloorPct` | 12000 | 22 |
| `chargeCommitCooldown` | 75 | 2600 |
| `stunChargeHoldMs` | 130 | 1050 |

**Impacto real acotado:** `defaults.cfg` fija por nombre casi todas esas claves al cargar, así
que en partida los valores correctos se restauraban. Las dos excepciones eran justamente
`chargeCommitCooldown` y `stunChargeHoldMs`, que sí están en `skill2.cfg` y `skill3.cfg` — o
sea que también quedaban tapadas. **Los defaults en C estaban mal, pero la configuración los
enmascaraba**: nada de lo que se observó en juego se explica por esto.

## 2. `botlite_combat_policy_t` — dos flags invertidos

`allowsDodgeIncoming` y `allowsOffensiveBreakLimit` estaban al revés respecto del struct. En
skill 2 eso significaba dodge apagado y break-limit encendido, exactamente lo contrario de lo
declarado en los comentarios. Otra vez, tapado porque los tres `.cfg` fijan ambas claves.

## La defensa que faltaba

Los dos bugs comparten la misma causa: **inicializadores posicionales que el compilador no puede
validar**, porque todos los campos son `int`/`float`/`qboolean` y encajan igual en cualquier
orden. lcc no emite ni un warning.

`Build.ps1` ahora comprueba, antes de compilar, que los comentarios `/* nombreDeCampo */` de
cada inicializador coincidan con el orden real del struct en el header — y aborta si no. Se
verificó reintroduciendo a propósito el swap de los dos flags: el guard lo detecta y corta
la compilación señalando las posiciones 18 y 19.

---

# Pendiente de validar en partida (Fase 5 y Fase 6)

Ninguna de las dos fases está probada en juego. Qué mirar en el log con `-debug`:

```
Engage intent -> RANGED score=-0.41 ki=72% sta=64% hp=88% dist=1840 tgt[chg=0 blk=1 kb=0]
Engage intent -> MELEE (forzado: lock de melee)
Engage kite: abriendo hueco dist=520 objetivo=900 inMelee=0
Tactic -> RANGED_KITE
Dodge incoming: SIDESTEP eta=480ms guided=0
```

Qué se esperaría ver, y qué significaría si no aparece:

- **Alternancia visible en skill 3.** Si el bot se queda pegado a una sola intención toda la
  pelea, el margen (0.22) es alto o los pesos están mal balanceados para las distancias reales
  del mapa.
- **`blk=1` seguido de un cambio a RANGED.** Es el caso más fácil de forzar a mano: bloquear
  sostenido debería empujar al bot a soltar el melee.
- **`tgt[chg=1]` seguido de un cambio a MELEE.** Cargar un ataque de ki delante del bot debería
  hacer que se meta encima a cortarlo.
- **Kiting.** Si `Engage kite` nunca aparece pero sí hay intenciones RANGED a corta distancia,
  `engage_kite_min_dist` (900) no se está alcanzando o el retroceso no rompe el lock.

Los valores de Fase 5 (radio de impacto 140, ventana 1200ms, umbral de zanzoken 350ms) y los
pesos de Fase 6 son **estimaciones razonadas, no medidas**. Es esperable que necesiten ajuste
tras verlos en acción. Los pesos están todos en `.cfg` y no requieren recompilar; las tres
constantes de Fase 5 siguen siendo `#define`.

# Orden de ejecución recomendado

```
T0.1 ──┬── T0.2  (quick win, riesgo cero)
       ├── T0.3 ── T0.4  (cvar en 0: sin cambio de comportamiento todavía)
       └── T1.1 ──┬── T1.2, T1.3                    ← Fase 1 jugable
                  │
                  └── T2.1 ── T2.2
                      T2.3, T2.4, T2.5, T2.6        ← Fase 2a jugable
                              │
                      ┌───────┴── FLIPEAR g_botStamina a 1 + re-tuning
                      │           (a partir de acá la stamina es real)
                      │
                      ├── T2.7 ── T2.8                ← Fase 2b: recuperación
                      │
                      └── T3.1 ── T3.2
                          T3.3, T3.4, T3.5
                          T3.6 (requiere T2.7+T2.8)   ← Fase 3 jugable
                                  │
                              T4.1, T4.2
```

**Momento de flipear la stamina:** al terminar T2.6 (Fase 2a), **antes** de T2.7/T2.8.

El razonamiento cambió al documentar la fórmula de recuperación: T2.7 (decidir si huir) y T2.8
(el ciclo ki↔stamina) **no se pueden desarrollar ni validar con stamina infinita** — son
literalmente respuestas a un recurso que hoy no existe. Desarrollarlas con el cvar en 0 sería
escribir a ciegas. Por eso el flip se adelanta: Fase 2a con stamina falsa, flip + re-tuning,
Fase 2b en adelante con stamina real.

**Hitos jugables:** al cerrar cada fase el juego debe ser jugable y las 3 skills distinguibles
entre sí. Ninguna fase deja el bot en estado intermedio roto.

---

# Crash en la primera prueba de Fase 6 (`/addbot piccolo 3 -debug`) — NO REPRODUCE

Log guardado en `crash-piccolo-20260922.log` (324 líneas, ~74 segundos de sesión).

## Lo que el log dice con certeza

- El proceso murió **sin mensaje de error del motor**. No hay `Com_Error`, no hay `ERR_DROP`,
  no hay señal. El log corta a mitad de frame.
- `vm_game "2"` → el QVM está **JIT-compilado** (`VM file game compiled to 1190037 bytes`).
  En ese modo un acceso inválido mata el proceso sin pasar por el manejo de errores del motor,
  que es exactamente el cuadro observado. Con `vm_game 1` (interpretado) el mismo acceso
  normalmente daría un error legible.
- La última línea es `SNAP targ ... dist=685`, o sea que el crash ocurrió **durante ese think**,
  después de loguear el snapshot.
- `SNAP self melee=DEGRESS` aparece **exactamente una vez en todo el log**, en la línea 323 de
  324 — el frame anterior al corte. Es el único momento en que el bot estuvo en
  `stMeleeDegressing`.

## Lo que NO está demostrado

Que el DEGRESS sea la causa. Es **una correlación con una sola muestra**. El kiting corrió dos
veces (líneas 261 y 320) y la primera no crasheó. Tampoco descarto la Fase 5: su barrido de
entidades corre todos los frames, aunque nunca encontró una amenaza (`Dodge incoming` = 0
ocurrencias).

No tengo stack trace y no puedo ejecutar el juego, así que no voy a afirmar una causa raíz.

## Por qué el DEGRESS es sospechoso de todas formas

`PM_Melee` corre entera con solo tener `lockedTarget > 0` (`bg_pmove.c:2336`), y con
`forwardmove < 0` pone al jugador en `stMeleeDegressing` (`bg_pmove.c:2354`). **Ningún bot había
retrocedido nunca con el lock puesto** — es un estado estrictamente nuevo introducido por el
kiting de Fase 6.

Además, `PM_Melee` tiene una desprotección real: el bloque que trabaja con `lockedPlayer` está
guardado por `if(pm->ps->lockedPlayer)` (línea 2345), pero justo después, en la línea 2372,
hace `enemyState = pm->ps->lockedPlayer->stats[stMeleeState]` **sin guardia**. Y `PM_SyncMelee`
(línea 2311-2312) deja deliberadamente a la víctima con `lockedTarget` puesto y
`lockedPlayer = 0`. O sea que el estado peligroso existe en el motor. Es pre-existente, no lo
introduje yo, pero el kiting podría estar llegando a él por un camino nuevo.

## Qué se cambió (independientemente del crash)

**Defecto real y visible en el log, corregido:** el kiting oscilaba cada tick. Líneas 319-322:
`RANGED_KITE → RANGED_PRESSURE → RANGED_KITE` en frames consecutivos, rozando los 900 de un
lado y del otro. La condición de entrada y la de salida eran la misma.

- Histéresis: entra por debajo de `engage_kite_min_dist`, sale recién al pasar ese umbral ×1.35,
  con 400ms de gracia. La condición vive ahora en `BotLite_EngageWantsKite`, que usan **tanto**
  la selección de táctica como la ejecución — antes cada lado decidía por su cuenta.
- **Se suelta el lock mientras retrocede.** El bot no lo necesita para disparar y
  `BotLite_RunRangedPressure` lo vuelve a tomar apenas el hueco está abierto. Esto elimina el
  estado `stMeleeDegressing` por completo. Es una hipótesis, no una corrección demostrada: si el
  crash reaparece, al menos queda descartada.

## Cómo aislarlo en la próxima prueba

1. **Kiting apagado** (sin recompilar): poner `uses_engage_kiting=0` en `skill3.cfg` y reiniciar
   el mapa. Si no crashea, está en ese camino. Si crashea igual, no es el kiting.
2. **Fase 5 apagada:** `uses_dodge_incoming=0` en `skill3.cfg`. Mismo razonamiento.
3. **Error legible:** `vm_game 1` en consola antes de cargar el mapa. Más lento, pero un acceso
   inválido debería salir como mensaje en vez de matar el proceso.

Con esos tres datos el problema queda acotado a un módulo.

---

# Segunda sesión: sin crash, todo activado — lectura del log

Log en `sesion-ok-20260922.log` (9831 líneas, dos bots: Piccolo skill 3 y vegetaCell skill 2).

**No crasheó.** Con la salvedad de siempre: el cambio del lock fue una hipótesis con una sola
muestra a favor y ahora una sola muestra en contra del crash. Dos muestras no son una prueba.
Si vuelve a aparecer, lo escrito en la sección anterior sigue siendo el punto de partida.

## Todo lo nuevo se activó por primera vez

| Módulo | Activaciones | Antes |
|---|---|---|
| `Dodge incoming` (Fase 5) | 15 | 0 — nunca había corrido |
| `Defensive zanzoken` (T2.5) | 14 | 0 |
| `Recover start` (T2.7) | 13 | 1 |
| `Engage intent` (Fase 6) | 37 | — |
| `Engage kite` (Fase 6) | 12 | — |
| `Tier escalate` (T2.3) | 31 | — |

## La alternancia funciona, y por las razones correctas

Las dos líneas que lo demuestran:

```
vegetaCell:2] Engage intent -> RANGED score=-0.22 ki=100% sta=97% hp=81%
              dist=782 tgt[chg=0 blk=1 kb=1]
```

A **782 unidades** — pegado al rival — eligió pelear a distancia, porque el rival estaba
bloqueando y en knockback. Con la lógica vieja (umbral 10000) eso era melee sin discusión.

```
Piccolo:1] Engage intent -> MELEE score=0.36 ki=4% sta=68% hp=28%
           dist=923 tgt[chg=0 blk=0 kb=0]
```

Con el ki al 4% eligió melee: el ataque de ki no pega y el Speed melee sale de la fatiga, que
estaba al 68%. Es exactamente el razonamiento que buscaba el término de ki.

Las anulaciones duras también dispararon: `MELEE (forzado: sin linea de vista)` y
`MELEE (forzado: lock de melee)`.

## Defecto encontrado y corregido: la Fase 5 gastaba zanzoken en golpes inevitables

De 14 esquives, **10 fueron zanzoken con eta de 5, 6, 8, 12, 16, 54, 58, 71, 148 y 234 ms**.

Un botón apretado ahora se aplica recién en el `ClientThink` siguiente (~50ms de frame de
servidor). Con eta de 5ms el proyectil impacta antes de que el teleport arranque: era stamina
tirada a la basura.

No es un fallo de la predicción. Es que el combate real de ZEQ2 pasa a corta distancia y los
haces son rápidos, así que el vuelo entero dura menos que la ventana de 1200ms que el módulo
mira. **La ventana de 1200ms es irrelevante en la práctica**; los únicos esquives con margen
real fueron los 4 SIDESTEP (374, 580, 856 y 1179 ms), todos a larga distancia.

Corrección: `BOTLITE_DODGE_MIN_REACTION_MS = 120`. Por debajo de eso no intenta nada y deja
correr la táctica normal.

## Oscilación del kiting: mejor, no resuelta

La histéresis funciona (`entra<900 sale>=1215`, y hay líneas de kite a `dist=909`, o sea
sosteniendo por encima del umbral de entrada). Pero todavía hay cambios `RANGED_KITE ↔
RANGED_PRESSURE` separados por ~10 líneas, contra 4 líneas consecutivas antes.

Parte del residuo es legítimo (el rival cerraba de 1545 a 811 en pocos frames, empujado por un
knockback), y parte puede ser que `kiteUntil` caduca cuando otro módulo del think loop se lleva
el frame antes de llegar al kite. No lo toqué: es de segundo orden comparado con lo demás.

## Lo que domina el log y no es mío

**8093 de 9831 líneas (82%) son `Bot died -> full respawn reset`.** Distribución de bloques
contiguos:

- 41 bloques de 124-125 ticks → respawn normal.
- **2 bloques de ~1250 ticks** → exactamente 10× más largo. El bot queda muerto ~10 veces más
  de lo normal y después revive solo.

O sea que el bot murió unas 47 veces en la sesión. Miré el contexto de una de las muertes:

```
SNAP self  ... ki=4% hp=311 tier=0
SNAP targ  ... ki=32767 hp=27952 tier=4 dist=30
```

Vida 311 sobre 32767 (0.9%), ki al 4%, tier 0 contra un rival en tier 4. No es un problema de
decisión: el bot estaba completamente superado.

Los 2 bloques de ~1250 ticks siguen siendo el bug de respawn pre-existente que ya estaba
anotado. No lo investigué — sigue sin causa raíz identificada.

## Consecuencia visible de morir tanto

`Recover timeout -> seeking nearest player stamina=100%` aparece 3 veces **con la vida todavía
en 28%**. El ciclo de recuperación recupera la fatiga pero no la vida, y sale por techo de
tiempo en vez de por umbral.

No es un bug del ciclo: curarse consume `plHealthPool`, que se llena con el daño que uno hace.
Un bot que pierde todos los intercambios no tiene pool que convertir. La recuperación de vida
depende de estar haciendo daño, y este bot no lo estaba haciendo.

---

# Fase 6.1 — El ciclo de recuperación nunca volvía a cargar ki ✅ CORREGIDO

Reportado en juego: *"cuando el bot baja de ki deja de pelear y no vuelve a cargar ki en ningún
momento, aun cuando la stamina ya se recuperó al 100"*.

Confirmado en el log del 22-09:

```
Recover timeout -> seeking nearest player stamina=100%
Recover start stamina=100% health=28% ki=4%
Recover timeout -> seeking nearest player stamina=100%
Recover start stamina=100% health=8%  ki=4%
```

Fatiga al 100%, ki clavado en 4% (exactamente `recover_ki_floor_hidden_pct`), entrando y
saliendo del modo sin pelear.

## Causa: una omisión mía, no un bug de tuning

El ciclo de T2.8 tenía tres etapas — DISENGAGE → DRAIN → REST — y **REST era terminal**:
`/* REST: sin botones, quieto. */ return qtrue;`. No existía la etapa de recarga.

La especificación original decía: *"luego volver a cargar para sanarse pero balanceando para que
la curación no genere más stamina de la deseada"*. **Implementé la primera mitad y nunca la
segunda.** El ciclo bajaba el ki y se quedaba ahí para siempre.

## Por qué además bloqueaba la vida

`plHealth` sólo sube cuando `plCurrent` ya llegó a `plMaximum` y se sigue cargando
(`bg_pmove.c:737-746`, rama `isBreakingLimit`), convirtiendo `plHealthPool` a razón de
`raise * 0.3`. Con el ki clavado en el piso eso no ocurre nunca.

Encadenado:

1. RECOVER baja el ki al piso y descansa.
2. La vida no puede subir porque requiere ki al 100%.
3. La salida exige `health >= recover_health_exit_pct` (70%) → inalcanzable.
4. Sale por techo de tiempo (12s).
5. Reentra al frame siguiente porque `health < recover_health_enter_pct` (40%).
6. Vuelta al punto 1, indefinidamente.

**El bot dejaba de pelear del todo.** Los dos bloques de ~1250 ticks muertos del log anterior
probablemente sean parte de este mismo cuadro.

## La corrección

**Cuarta etapa, RECHARGE.** Después de descansar hasta `recover_rest_exit_pct` (85% de fatiga),
sostiene POWERLEVEL + `rightmove > 0` para recargar.

El balance que pedía la especificación sale de una asimetría real del motor, no de un número
inventado: **cargar ki por debajo del 100% no cuesta fatiga** — todos los `plUseFatigue` de esa
rama están dentro del `if (plCurrent == plMaximum)`. Lo que sí hace todo el tramo es *frenar* la
recuperación de fatiga, porque `usingAlter` la anula (`bg_pmove.c:647`). O sea: subir a 100% es
barato, empujar más allá para curarse es lo que cuesta. Por eso `recover_recharge_min_stamina_pct`
(45%) sólo corta el empuje y devuelve a REST, en vez de prohibir la carga entera.

**Detección de pool vacío.** `plHealthPool` se llena con el daño que uno hace
(`bg_pmove.c:2479`, `2594`). Un bot que viene perdiendo todos los intercambios no tiene nada que
convertir, y esperar la curación es tiempo tirado. Si el ki ya está al 100% y la vida no se movió
en 2500ms, sale a pelear en vez de seguir esperando.

**Enfriamiento entre episodios** (`recover_cooldown_ms`, 4000). Sin esto, una salida por timeout
con la vida todavía baja reentraba al frame siguiente — el bucle de arriba.

## Líneas nuevas en el log

```
Recover: stamina 87%, recargando ki
Recover: sin pool para curar, volviendo al combate
Recover: stamina 41% muy baja para recargar, descansando
Recover: ki al 70%, volviendo al combate      (sólo si recharge_ki_pct < 100)
```

Si aparece `sin pool para curar` muy seguido, el bot no está conectando golpes — es un problema
de balance del enfrentamiento, no del ciclo.

---

# Fase 6.2 — El zanzoken nunca llegó a ejecutarse ✅ CORREGIDO

Reportado en juego por tercera vez. Las correcciones anteriores (T2.5, cuatro iteraciones)
fueron **todas sobre cuándo decidirlo**. El problema real era **cómo se ejecuta**, y nunca lo
miré.

## Cómo funciona de verdad `PM_CheckZanzoken` (`bg_pmove.c:307-350`)

```
1) Al soltar el boton:       tmZanzoken = -1   (queda rearmado)
2) Al apretarlo con -1:      usingZanzoken = on
                             tmZanzoken = plFatigue / 93.62 + stZanzokenDistance
                             cobra plMaximum * 0.12 * stZanzokenCost de fatiga
3) Mientras tmZanzoken > 0:  VectorNormalize(velocity)
                             VectorScale(velocity, speed)
4) usingZanzoken con tmZanzoken <= 0 y nada delante:
                             PM_StopZanzoken() -> VectorClear(velocity)
```

## Los dos requisitos que el bot no cumplía

**A) Hay que sostener el botón.** Un toque de un frame entra por el paso 2; al frame siguiente,
con el botón ya suelto, el paso 1 pone `tmZanzoken = -1`, lo que dispara el paso 4 y **frena en
seco**. Se pagaba el costo completo de fatiga para desplazarse prácticamente nada.

Eso es exactamente lo que hacía `BotLite_EA_Button( bot, BUTTON_TELEPORT )` suelto, porque
`BotLite_ActionReset` limpia la acción al inicio de cada think.

**B) Hace falta una dirección.** El paso 3 no teletransporta a ningún lado: **normaliza la
velocidad que ya se tiene** y la escala. Con el bot quieto esa velocidad es cero, normalizar
cero da cero y escalar cero da cero. De los tres puntos donde el bot apretaba teleport, **dos no
mandaban ningún movimiento** (escape defensivo y esquive de proyectil); sólo el de aproximación
mandaba `MoveForward`.

O sea que el zanzoken de aproximación era el único que hacía *algo*, y aun así cortado al frame
siguiente.

## Precisión sobre los recursos

La **duración** sale de la fatiga (`plFatigue / 93.62`) y el **costo** también es fatiga
(`plMaximum * 0.12 * stZanzokenCost`, con 20% de descuento con lock y 60% con salto).
Lo que escala con **ki** es la **velocidad** (`plCurrent / 13.1`, `bg_pmove.c:330`).

Con la fatiga en el piso el zanzoken dura nada aunque sobre el ki; con el ki bajo dura igual pero
llega mucho menos lejos. Con `plFatigue` al máximo (32767) la duración base son ~350ms más
`stZanzokenDistance` del personaje.

## La corrección

Módulo nuevo `g_botlite_zanzoken.c` con un sostenido compartido:

- `BotLite_ZanzokenDurationMs` replica la fórmula del motor para saber cuánto va a durar **antes**
  de apretar (topes de cordura en 120-1500ms).
- `BotLite_StartZanzoken( bot, clientNum, fwd, right, up )` arranca el sostenido con una dirección
  obligatoria; si le pasan las tres en cero, fuerza adelante en vez de gastar fatiga para quedarse
  quieto.
- `BotLite_RunZanzokenHold` corre **primero en el think loop**, antes incluso del esquive de
  Fase 5. Cualquier módulo que se lleve el frame suelta el botón y aborta el desplazamiento a
  mitad de camino.

Direcciones por caso de uso:

| Uso | Dirección | Por qué |
|---|---|---|
| Escape defensivo (T2.5) | gira en sentido opuesto al rival y avanza | Retroceder de frente (`forwardmove < 0`) con el lock puesto pone al bot en `stMeleeDegressing` — el estado que coincidió con el crash del 22-09. |
| Esquive de proyectil (Fase 5) | lateral (`rightmove ±127`) | Teletransportarse a lo largo de la trayectoria no esquiva nada. `rightmove` tampoco dispara DEGRESS. |
| Aproximación (melee) | adelante | Ya está mirando al rival. |

Ya no queda ningún `BUTTON_TELEPORT` suelto fuera del helper.

## Líneas nuevas en el log

```
Zanzoken start: 412ms fwd=127 right=0 up=0 fatiga=88%
Zanzoken cortado: estado del motor lo bloquea
Zanzoken cortado: sin fatiga
```

Si `Zanzoken start` aparece y aun así no se ve el desplazamiento, el problema pasa a ser la
velocidad previa del bot en ese instante — el paso 3 necesita que ya se esté moviendo, y arrancar
desde quieto da poco aunque el botón se sostenga bien.

---

# Fase 6.3 — Llegada horizontal al melee ✅ HECHO

Pedido: *"cuando el bot busque al target para melee, trate de llegar en un ángulo lo más
horizontal posible, ya que en muchas ocasiones suele irse un poco para arriba y se desfasa la
vista de la cámara"*.

## Por qué pasaba

`PM_FlyMove` arma la velocidad con los vectores de la **vista** (`bg_pmove.c:1300`):

```c
wishvel[i] = scale * pml.forward[i] * forwardmove
           + scale * pml.right[i]   * rightmove
           + scale * pml.up[i]      * upmove;
```

`pml.forward/right/up` salen de `AngleVectors(viewangles, ...)`. El bot apuntaba al rival con
`BotLite_FaceTarget`, que incluye el pitch. Con pitch ≠ 0 el "adelante" **tiene componente
vertical**: avanzar es también trepar o picar. Por eso llegaba al duelo subiendo.

Con el pitch en 0 la cosa se separa limpio: `forward` queda horizontal y `up` queda alineado con
el eje Z del mundo. Entonces `forwardmove` cierra sólo la distancia horizontal y `upmove` sólo la
vertical.

## La corrección

**`BotLite_FaceTargetLeveled( bot, target, maxPitch )`** — apunta al rival pero recorta el pitch
al tope configurado (0 = completamente horizontal).

**`BotLite_ApplyApproachMovement`** ahora recibe el snapshot y corrige la altura con `upmove`
en vez de con el pitch, con banda muerta para que no oscile al llegar.

**Aproximación empinada.** Si la diferencia de altura supera la distancia horizontal por
`melee_approach_steep_ratio` (0.45, unos 24°), el avance horizontal baja a 40 para que el bot
**iguale altura primero** y haga el último tramo plano. Sin esto seguiría llegando en diagonal,
sólo que con el cuerpo derecho.

**Sólo mientras se acerca.** `BotLite_BeginMeleePressure` usa la orientación nivelada únicamente
si `!botInMelee`; ya enganchado en el duelo, la orientación la maneja el motor y forzarla sería
pelearse con él.

Se aplicó también a la táctica `APPROACH` del despachador (`BotLite_ApproachTargetLeveled`), que
tenía su propio `FaceTarget` + `MoveForward` duplicado.

## Claves nuevas

| Clave | Default | Qué hace |
|---|---|---|
| `melee_approach_max_pitch` | 0 | Inclinación máxima permitida al acercarse. 0 = horizontal puro. |
| `melee_approach_level_tolerance` | 48 | Banda muerta de altura, en unidades. |
| `melee_approach_steep_ratio` | 0.45 | Proporción altura/distancia a partir de la cual iguala altura antes de cerrar. |

Si queda demasiado rígido, subir `melee_approach_max_pitch` a 10-15 devuelve algo de inclinación
sin volver al problema original.

---

# Fase 6.4 — El bot se colgaba reintentando transformarse ✅ CORREGIDO

Reportado: *"llega un momento en el cual dejan de usar ki boost y transformarse, aun teniendo
ki. Y si bien hay veces que esquivan los ataques a distancia, no siempre funcionan"*.

Log en `sesion-tierspam-20260922.log`.

## No dejaba de transformarse: no paraba de intentarlo

`Tier escalate from=...` aparece **4637 veces** en una sesión (contra 31 en la anterior). Y
siempre con los mismos valores:

```
1786 Tier escalate from=3 targetTier=4 hp=18523
1509 Tier escalate from=3 targetTier=4 hp=32767
```

La secuencia de tiers del bot, comprimida:

```
4 x21   3 x51   4 x17   3 x54   3 x1  4 x1  3 x1  4 x1  3 x1  4 x1  ...
```

**Sube a 4, el motor lo baja a 3, vuelve a subir, lo baja.** Un snapshot por vez.

## Por qué

`checkTier` corre cada 300ms (`PM_CheckTransform`, `bg_pmove.c:589`) y baja de tier en cuanto
cualquier recurso cae por debajo del `sustain` del tier actual (`g_tiers.c:88-97`).

El caso concreto: goku tier 4 pide `sustainFatigue 8000` **y además drena fatiga mientras está
activo** (`effectFatigue -15`). Subir con la fatiga en 8001 garantiza la caída pocos frames
después. Al caer a tier 3 (`sustainFatigue 4000`) la fatiga se recupera, cruza 8000, y sube otra
vez. Un ciclo límite perfecto.

## La parte que era mía, y la que explica el síntoma

`BotLite_RunTransformControl` devolvía `qtrue` —consumiendo el frame entero— **en cuanto había
motivo para escalar, sin mirar si la escalada era posible**. No comprobaba ninguno de los
`requirement*` del motor ni tenía límite de frecuencia. `transform_interval_ms` estaba
documentado en el README y en `defaults.cfg` desde T1.1, pero **nunca se leía**: no existía en el
parser ni en el struct de perfil.

Consecuencia: el bot gastaba **todos** los frames apretando el botón de transformación. Sin
atacar, sin boost, sin cargar ki — o sea sin poder juntar nunca los recursos que le faltaban.
De ahí el "dejan de usar ki boost y transformarse aun teniendo ki": no es que dejaran de
transformarse, es que no hacían **nada más**.

## La corrección

- **`BotLite_TierEscalationViable`** comprueba los `requirement*` duros del motor y, encima, exige
  los `sustain*` del tier siguiente **con margen** (`tier_escalate_margin_pct`, 130%). Subir justo
  al límite es subir para caerse.
- **Los dos `return qfalse` nuevos no consumen el frame.** Es el punto central: cediendo el
  frame, el bot vuelve a pelear y a cargar, que es lo único que puede acercarlo al tier siguiente.
- **`transform_interval_ms` ahora se lee de verdad** (parser + struct + default 20000).
- Log de postergación limitado a uno cada 5s, para no repetir el diluvio de 4637 líneas.

## Esquive: por qué funcionaba "a veces"

El piso de 120ms de la corrección anterior funcionó — ya no hay zanzokens con eta de 5ms; el
mínimo ahora es 143ms. Pero mirando qué esquiva:

| | guided=0 | guided=1 |
|---|---|---|
| SIDESTEP | 6 | 13 |
| ZANZOKEN | 3 | 1 |

**La mayoría de las amenazas son guiadas, y casi todas salían como paso lateral.** `EF_GUIDED` es
un haz que el que dispara sigue dirigiendo (`g_usermissile.c:952-953`): corrige rumbo y acompaña
el desplazamiento lateral. Un paso al costado no lo pierde. Lo único que lo pierde es un salto
grande e instantáneo.

Corregido: contra una amenaza guiada ahora se usa zanzoken **siempre**, sin esperar a que la
ventana baje de 350ms.

## Tope del zanzoken que estaba recortando de más

Varios `Zanzoken start` arrancaban exactamente en **1500ms** — mi tope de cordura, no el valor
real. `stats[stZanzokenDistance]` es `zanzokenDistance` del tier por 500 (`g_tiers.c:28`), así que
con 1.6 ya son 800ms de base más los ~350 de la fatiga llena. El tope subió a 3000: está para
atajar un stat corrupto, no para recortar duraciones legítimas.

## Claves nuevas

| Clave | Default | Qué hace |
|---|---|---|
| `tier_escalate_margin_pct` | 130 | Margen exigido sobre el sustain del tier siguiente. |
| `transform_interval_ms` | 20000 | Ya existía en el cfg; ahora se lee. |

---

# Fase 6.5 — La corrección anterior dejó al bot sin boost ni sanzoken ✅ CORREGIDO

Reportado: *"ahora nunca se transforma ni hace ki boost"*. Log en
`sesion-notier-20260922.log`.

Lo bueno primero: el spam desapareció. Cero líneas `Tier escalate` (contra 4637), y el bot por fin
hace cosas — `Charge commit` 31, `Reactive defense` 34, `Combat state -> SPECIAL` 16,
`Defensive zanzoken` 14, `Engage kite` 47. Los frames que antes se comían los reintentos ahora se
usan para pelear.

Pero aparecieron dos problemas, y el del boost lo causé yo.

## 1. El boost estaba atado a un flag que ya no significa nada

`BotLite_ShouldUseBoost` y `BotLite_ShouldUseSanzoken` tenían, sólo para skill 3:

```c
if ( info->skill == 3 ) {
    if ( !info->ranged.didInitialTransform ) { return qfalse; }
    ...
}
```

`didInitialTransform` es un resto del modelo viejo, donde skill 3 se transformaba una vez al abrir
combate y recién después se le habilitaba lo demás. **T2.3 reemplazó eso por gestión continua de
tiers** y el flag dejó de tener un momento claro en el que encenderse: hoy sólo pasa a `qtrue`
cuando el bot ya llegó a su tier máximo.

Mientras el bot escalaba a los golpes eso pasaba desapercibido — terminaba llegando al tope y
desbloqueaba todo. Al limitar los intentos (Fase 6.4) el bot se quedó en tier 0, el flag nunca se
encendió, y **skill 3 quedó sin boost NI sanzoken durante toda la partida**.

Corregido: se eliminó esa compuerta en los dos lugares. Queda sólo el chequeo de
`BOTACT_TRANSFORMING`, que sí tiene sentido (no boostear en mitad de la animación).

## 2. Nunca se transformaba porque el rival tampoco

Los tiers del log: **436 snapshots, todos `tier=0`**, contra un rival en `tier=0` (416 de 423).

`BotLite_ShouldEscalateTier` sólo tenía dos disparadores: que el rival esté en un tier más alto, o
que la vida propia baje del 75%. Con un rival que no se transforma y que no llega a bajarle la
vida, ninguno se cumple nunca.

Que la transformación sea una respuesta al combate no quiere decir que tenga que ser una
respuesta a **estar perdiendo**. Se agregó un tercer disparador: **combate sostenido contra el
mismo objetivo** (`tier_escalate_combat_ms`, 8000). No es un ritual de apertura — pide pelea
continuada, y la viabilidad con margen de la Fase 6.4 sigue decidiendo si de verdad se puede.

## Diagnóstico agregado

Van dos reportes de "no hace boost" sin una sola línea en el log para mirar. Se agregó
`Boost bloqueado: presupuesto de stamina (NN%)`, limitado a una cada 5s. Es el único gate del
boost que depende de un valor variable; el resto son estados binarios que ya se ven en `SNAP`.

## Clave nueva

| Clave | Default | Qué hace |
|---|---|---|
| `tier_escalate_combat_ms` | 8000 | Combate sostenido tras el cual escalar se justifica solo. 0 lo desactiva. |

---

# Fase 6.6 — El bot escapaba en vez de pelear melee ✅ CORREGIDO

Pedido: *"el bot tiende a evadirme mucho en melee en lugar de pelear. Lo de evadir y sanzoken
solo debe hacerlo si el bot se encuentra en un estado crítico o con poderes a distancia. Si no,
no tiende a pelear melee nunca"*.

## La evidencia

Estados de melee del bot en toda la sesión:

```
128 melee=INACTIVE    58 melee=AGGRESS    6 melee=START_ATTACK    4 melee=START_HIT
```

**Ni un solo frame en un estado real de duelo** — nada de `USING_SPEED`, `USING_POWER`,
`USING_BLOCK`, `IDLE`. Y las tácticas: `RANGED_PRESSURE` 26 + `RANGED_KITE` 22 = **48 contra 10
de melee**.

## Tres causas

### 1. El Evade era el default, no la excepción

```c
int evadeChance = 100 - (int)( targetSpeedBreakerBias * 70.0f );
```

Con `bias = 0` eso da **100**: el bot *siempre* esquivaba en vez de bloquear, y sólo bajaba si el
rival abusaba del Speed Breaker. Bloquear mantiene al bot dentro del intercambio; esquivar lo
saca.

Ahora el Evade exige estado crítico.

### 2. El zanzoken defensivo se disparaba con casi cualquier cosa

`BotLite_ZanzokenThreatPresent` aceptaba "desventaja de vida y rival cerca", que en la práctica es
casi todo el tiempo que el bot va perdiendo. Los 14 escapes del log tienen todos `kb=0` y
`targetCharging=0`: todos venían de esa condición. **Cada vez que el jugador cerraba a melee, el
bot se teletransportaba.**

También se quitó el disparador por carga de melee del rival: a un Power o Stun cargado se le
responde con Block o con Charge Breaker, que el bot ya tiene desde T2.2. Teletransportarse era
renunciar al intercambio.

Ahora exige estado crítico real. Las amenazas a distancia quedan cubiertas por la Fase 5, que es
el otro caso que el pedido habilita explícitamente.

### 3. La razón de fondo: el score empujaba a ranged con el bot sano

Esto no estaba en el pedido, pero es por qué "no pelea melee nunca" y no se arreglaba sólo con
lo anterior.

Los términos de **ki** y **vida** eran simétricos. Con el bot al 100% de ambos:

| Término | Aporte | Peso | Total |
|---|---|---|---|
| ki | −1.00 | 1.10 | **−1.10** |
| vida | −1.00 | 0.90 | **−0.90** |

**−2.00 sobre una suma de pesos de 6.40**, antes de mirar nada del combate. Un bot sano y cargado
arrancaba estructuralmente inclinado a pelear de lejos.

Esa mitad nunca estuvo justificada. Escribí "ki bajo → melee" porque el ataque de ki pega poco y
cuesta ki; pero lo inverso —tener mucho ki— hace que el ranged sea **asequible**, que no es lo
mismo que hacer que el melee sea mala idea. Igual con la vida: estar sano no es motivo para pelear
de lejos; si acaso es lo contrario, porque el melee es el mejor convertidor de daño en vida del
juego (1.0/0.8 contra 0.7/0.3).

Ambos términos pasaron a ser **unilaterales**: empujan a melee cuando el recurso está bajo, y
aportan 0 cuando está alto.

## Clave nueva

| Clave | Default | Qué hace |
|---|---|---|
| `escape_critical_health_pct` | 30 | Vida bajo la cual se habilitan Evade y zanzoken defensivo. También cuenta como crítico que la fatiga entre en CRÍTICO/AGOTADO. |

## Qué mirar

`Defensive zanzoken` y `Reactive defense ... -> EVADE` deberían volverse raros y aparecer sólo con
la vida baja. Y en `SNAP self` deberían empezar a verse estados de duelo reales
(`USING_SPEED`, `USING_POWER`, `USING_BLOCK`). Si siguen sin aparecer, el problema restante está
en el enganche del lock, no en la decisión.

---

# Fase 6.7 — La transformación nunca aterrizaba, y el boost sin diagnóstico ✅

Reportado: *"el bot solo usa ki boost en el skill 2, en el 3 no lo usa nunca. Y ahora ha dejado
de transformarse, antes sí lo hacía. Quiero que se pueda transformar independientemente del tier
del target"*. Aclaración del usuario sobre qué es el ki boost: *"cuando se cargan los poderes con
más rapidez o se vuela más rápido"* — o sea `BUTTON_BOOST` / `usingBoost`.

Log en `sesion-tier0-20260922.log`.

Primero, lo que sí mejoró con la Fase 6.6: `Combo advanced stage` 221 y
`Combat state -> MELEE_COMBO` 164, contra 0 antes. **El bot ahora pelea melee de verdad.**

## La transformación: apretaba el botón y no pasaba nada

```
Tier escalate from ...........  23 veces
SNAP self tier=0 .............  1101 de 1101 snapshots
```

23 pulsaciones, cero transformaciones. No era la decisión — era la ejecución.

**Mismo problema que tenía el zanzoken, por un camino distinto.** `checkTier` no corre todos los
frames: `PM_CheckTransform` (`bg_pmove.c:587-595`) emite `EV_TIERCHECK` **una vez cada 300ms**, y
`g_active.c:405` recién ahí llama a `checkTier`. Por el otro lado, `keyTierUp` se pone y se saca
dentro del mismo `PM_CheckPowerLevel` (`bg_pmove.c:692-715`): en cuanto el botón se suelta, el
flag se borra.

Un toque de un frame (50ms) sólo sirve si cae justo en el tick de los 300ms. Corregido con un
sostenido de 450ms, que garantiza que el tick caiga dentro. Se aplicó también a la bajada de
tier.

Se agregó `Tier cambio: N -> M`, que se emite sólo cuando `plTierCurrent` cambia de verdad. Hasta
ahora no había forma de distinguir en el log "aprieta el botón" de "se transforma".

## Transformarse independientemente del tier del rival

El tercer disparador de la Fase 6.5 (combate sostenido) ya no mira al rival, pero su reloj se
reiniciaba cada vez que el objetivo se perdía un instante. Ahora el reloj **sólo se borra al
morir**: perder al rival tras una esquina o durante su respawn ya no cancela que el bot viene
peleando hace rato.

Quien decide si de verdad se puede sigue siendo `BotLite_TierEscalationViable`, que exige los
requisitos del motor y el sustain del tier siguiente con margen. O sea que esto no empuja al bot
a subir a un tier que no pueda sostener.

## Corrección a una especulación anterior

En la Fase 6.4 escribí que `requirementButton` y `permanent` quedaban en 0 por defecto y que eso
hacía que `checkTier` bajara de tier incondicionalmente por un problema de precedencia en
`g_tiers.c:88`. **Era falso.** `setupTiers` carga `players/tierDefault.cfg` antes del archivo de
cada tier, y ese default trae `requirementButton True` y `tierPermanent False`. Con
`requirementButton` en verdadero la rama de descenso sí exige `keyTierDown`. No hay bug de
precedencia observable.

Vale la pena anotar la estructura, porque es fácil equivocarse: **`client->tiers[i]` se carga
desde la carpeta `tier{i+1}/`**. `tiers[0]` es la forma base. Un bot en `plTierCurrent = 0`
consulta `tiers[1]`, que sale de `tier2/`.

## El boost: tres reportes sin una sola línea de log

El diagnóstico que agregué en la Fase 6.5 cubría **sólo** el presupuesto de stamina, y en el log
disparó **0 veces**. O sea que gasté el ciclo anterior sin obtener nada.

Ahora se nombra el primer gate que corta, de entre los 17 flags de estado más los tres timers de
melee:

```
Boost bloqueado: USING_MELEE
Boost bloqueado: WEAPON_BUSY
Boost bloqueado: el tier actual no habilita canBoost
Boost bloqueado: combo en curso
```

Se quitó además un chequeo redundante: había un `if (skill == 3 && TRANSFORMING) return qfalse`
justo antes de la lista general, que **ya incluye `BOTACT_TRANSFORMING`**. No cambiaba nada, pero
hacía parecer que skill 3 tenía una restricción propia de boost cuando no la tiene.

**No afirmo haber arreglado el boost de skill 3** — no tengo evidencia de cuál es el gate. Lo que
sí sé es que `canBoost` viene de `tierDefault.cfg` con `True`, así que la capacidad existe en
todos los tiers. La hipótesis más probable es que skill 3, al pelear mucho más melee y cargar
mucho más, viva casi siempre en `USING_MELEE` / `CHARGING` / `WEAPON_BUSY`. La próxima corrida lo
dirá con nombre propio.
