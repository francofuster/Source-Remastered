# Plan de pruebas — Fase 0 + T1.1 (+ Fase 6 al final)

Estado del código al momento de escribir esto: **T0.1, T0.2, T0.3, T0.4 y T1.1 implementadas**.
`g_botStamina` está en **0** por defecto, o sea la stamina real sigue apagada a propósito.

## Compilar

```
cd D:\ZEQ2\Remaster\Source-Remastered
.\Build.bat
```

El script verifica que los tres `.qvm` se hayan regenerado de verdad (compara fecha antes/después).
Si un módulo falla, los `.bat` igual terminan con código 0 y borran los `.asm`, así que **sin esa
verificación una compilación rota se ve idéntica a una exitosa** y el juego seguiría cargando el
QVM viejo. Si algo falla, corré el `.bat` de ese módulo a mano para ver el error real.

## Preparar la sesión

1. Arrancar el juego y cargar cualquier mapa como servidor local.
2. Abrir la consola con `~`.
3. Los logs de bot salen por `G_Printf`, o sea a la **consola del servidor**. En partida local es
   la misma consola. Para guardarlos: `condump bots.txt`.

Agregar bots (no requiere cheats):

```
/addbot nappa 1 -debug     → skill 1, archetype bruiser  (block_tendency 0.22)
/addbot frieza 2 -debug    → skill 2, archetype keepaway (block_tendency 0.50)
/addbot goku 3 -debug      → skill 3, archetype custom   (block_tendency 0.60)
/removebot nappa
```

---

## 1. No-regresión (hacer esto primero)

Si algo de acá falla, no sigas con el resto: hay un problema de base.

| # | Prueba | Esperado |
|---|---|---|
| 1.1 | `/addbot goku 3` y pelear un rato | El bot se comporta **igual que antes** de todos estos cambios |
| 1.2 | Agregar los 3 bots a la vez | Ninguno crashea ni se queda tildado |
| 1.3 | Matar un bot y esperar respawn | Vuelve a pelear normalmente |
| 1.4 | `/removebot <nombre>` | Se va limpio, sin error |

Con `g_botStamina 0` el único cambio de comportamiento visible debería ser el **bloqueo reactivo**
(sección 3). Todo lo demás es infraestructura.

---

## 2. T0.1 — Snapshot de percepción

Con `-debug`, cada segundo deberían salir dos líneas por bot:

```
SNAP self melee=IDLE chg=0 kb=0 strug=0 fatigue=100%(COMFORTABLE) ki=87% hp=... tier=1
SNAP targ melee=CHARGING_POWER chg=310 kb=0 strug=0 ki=... hp=... tier=0 dist=52 los=1
```

| # | Prueba | Esperado |
|---|---|---|
| 2.1 | Mirar las líneas `SNAP` | Aparecen ~1 por segundo, no spamean |
| 2.2 | Alejarte del bot | `SNAP targ` desaparece y sale `SNAP target none` |
| 2.3 | Ponerte detrás de una pared | `los=0` |
| 2.4 | Entrar en melee y **mantener** alt-attack | `targ melee=` pasa a `CHARGING_POWER` y `chg=` sube hasta ~550 |
| 2.5 | Mantener attack normal en melee | `targ melee=CHARGING_STUN`, `chg=` sube hasta ~1000 |
| 2.6 | Recibir un knockback del bot | En tu lado no se ve, pero `SNAP self ... kb=` del bot sube a ~5000 cuando vos se lo hacés |
| 2.7 | Transformarte | `targ tier=` sube |

**Lo importante de esta sección:** que `targ melee=` refleje lo que realmente estás haciendo.
Es el dato sobre el que se construye todo lo reactivo.

---

## 3. T1.1 — Bloqueo reactivo ⭐ (el cambio más visible)

**Probar con skill 1 o 2**, no con skill 3. Skill 3 todavía tiene además el bloqueo por
temporizador aleatorio (`uses_block=1`), que ensucia la lectura. Skill 1 y 2 tienen
`uses_block=0`, así que **cualquier bloqueo que veas es reactivo**.

| # | Prueba | Esperado |
|---|---|---|
| 3.1 | Melee con nappa (skill 1), cargar Power | Sale `Reactive block: target=CHARGING_POWER chance=22% -> ignore` (o `-> BLOCK`) |
| 3.2 | Repetir varias cargas | **Una sola línea por carga**, no una por frame. Si spamea, el control de episodio está roto |
| 3.3 | Contar sobre ~20 cargas con nappa | Bloquea ~1 de cada 5 (bruiser, 0.22) |
| 3.4 | Lo mismo con frieza (skill 2, 0.50) | Bloquea ~la mitad |
| 3.5 | **No cargar nada**, solo golpes rápidos | **Cero** líneas `Reactive block`. Si aparecen, está disparando con el estado equivocado |
| 3.6 | Comparar tiempo de reacción skill 1 vs skill 2 | Skill 1 llega notoriamente más tarde; a veces la carga termina antes y no bloquea |

**El detalle que más vale mirar:** con skill 1 la reacción es 380-750ms y una carga de Power dura
550ms. O sea que **skill 1 a menudo llega tarde y pierde la ventana**. Eso debería *sentirse* como
un bot lento, no solo como un bot que bloquea poco. Si se siente igual que skill 2, hay que subir
más el retardo en `botsys/skills/skill1.cfg`.

---

## 4. T0.2 — Forcejeo de haces

Necesitás chocar tu haz contra el del bot. Lo más fácil es con un personaje de ataque de rayo
continuo, apuntando al bot mientras él también dispara.

| # | Prueba | Esperado |
|---|---|---|
| 4.1 | Provocar un choque de haces | Sale `Struggle started beam=N enemyBeam=N ki=NN% boosted=1` |
| 4.2 | Mirar el campo **`boosted=`** | Debe ser **1** si el bot tenía ki suficiente. **Si siempre es 0, `BotLite_StrugglePrepare` no está funcionando** y el bot va a perder todos los forcejeos |
| 4.3 | Terminar el forcejeo | Sale `Struggle ended after NNNNms` |
| 4.4 | Repetir con skill 1 vs skill 3 | Skill 1 solo se compromete con ki ≥75%, skill 3 con ≥20% → skill 3 disputa mucho más seguido |
| 4.5 | Ganar/perder forcejeos | El bot ahora debería ganar algunos. Antes perdía **siempre** |

**Contexto:** el boost queda congelado en el instante del choque (`PM_CheckBoost` hace
early-return mientras `isStruggling`), así que `boosted=` refleja una decisión tomada *antes*
del choque. Ese campo es la validación real de la tarea.

---

## 5. T0.3 / T0.4 — Stamina (exploratorio, no esperar que esté balanceado)

### 5a. Con el cvar apagado (estado por defecto)

| # | Prueba | Esperado |
|---|---|---|
| 5.1 | `/g_botStamina` (ver valor) | `0` |
| 5.2 | Mirar `SNAP self` en combate largo | Siempre `fatigue=100%(COMFORTABLE)` |
| 5.3 | Comportamiento general | Idéntico al de antes de estos cambios |

### 5b. Con el cvar encendido — **esto todavía NO está tuneado**

```
/g_botStamina 1
/map <el mismo mapa>      ← hace falta recargar para que aplique limpio
```

Esto es una prueba de humo, no una validación de balance. Según el backlog el flip real va
después de la Fase 2. Lo que quiero saber ahora es si el mecanismo funciona:

| # | Prueba | Qué observar |
|---|---|---|
| 5.4 | Pelear 1-2 minutos | `fatigue=` **baja** y el estado pasa `COMFORTABLE → TIGHT → CRITICAL` |
| 5.5 | Dejar al bot tranquilo | La fatiga se **recupera** sola |
| 5.6 | Bot en `CRITICAL` | Deja de boostear (gasto CHEAP cortado) |
| 5.7 | Bot en `TIGHT` | Deja de usar zanzoken (gasto EXPENSIVE cortado) |
| 5.8 | **Combate largo, 3+ min** | ⚠️ ¿El bot llega a fatiga 0 y **empieza a perder vida**? (`bg_pmove.c:375-381`) |
| 5.9 | Bot agotado | ⚠️ ¿Queda permanentemente lento? La velocidad escala con fatiga (`bg_pmove.c:998`) |
| 5.10 | Bot con tier > 1 | ⚠️ ¿Se **des-transforma solo** al caer la fatiga? (`sustainFatigue`, `g_tiers.c:73`) |

Los tres con ⚠️ son los riesgos que anoté en el backlog. **Si pasan, no es un bug del código
sino la razón por la que T2.7/T2.8 (decidir retirarse y recuperarse) existen** — el bot todavía
no sabe gestionar ese recurso. Anotá qué tan rápido pasa, sirve para tunear los umbrales.

Volver a apagarlo con `/g_botStamina 0` + recargar mapa.

---

## Qué reportar

Para cada cosa que falle, lo más útil es:
1. Qué número de prueba
2. Las líneas de log alrededor del momento (`condump`)
3. Skill y personaje del bot

Lo que más me interesa saber primero: **3.2** (una línea por carga, no spam),
**3.5** (que no dispare sin carga) y **4.2** (`boosted=1`). Esos tres validan que la lógica
nueva está leyendo bien el estado del rival, que es la base de todo lo que sigue.

---

## 6. Fase 6 — Alternar melee / ranged por decisión

Usar **skill 3** con `-debug`. La línea clave del log es `Engage intent -> ...`.

**6.1 — Que alterne.** Pelear normal 2-3 minutos. Contar cuántas veces aparece
`Engage intent ->` con un valor distinto al anterior.

- Si aparece **una sola vez** en toda la pelea: el margen (`engage_switch_margin=0.22`)
  es demasiado alto para el rango de scores que se dan en ese mapa, o los pesos están
  desbalanceados. Bajar el margen a 0.12 y volver a mirar.
- Si aparece **cada pocos segundos**: subir el margen o `engage_switch_min_ms`.

**6.2 — Bloquear debería empujarlo a distancia.** Mantener BLOQUEO sostenido mientras el
bot pelea de cerca. Esperado: tras un par de segundos, `Engage intent -> RANGED` con
`blk=1` en la misma línea. Es el caso más fácil de forzar a mano.

**6.3 — Cargar ki debería atraerlo.** Ponerse a distancia media y cargar un ataque de ki
delante del bot. Esperado: `Engage intent -> MELEE` con `chg=1`, y que se meta encima a
cortar la carga.

**6.4 — Kiting.** Con el bot en intención RANGED, acercarse sin llegar a agarrarlo en
melee (entre ~100 y ~900 unidades). Esperado: `Engage kite: abriendo hueco dist=...` y
que retroceda de frente, sin dar la espalda.

- Si nunca aparece pero sí hay intenciones RANGED a corta distancia: revisar
  `engage_kite_min_dist` (900).
- **Nota:** dentro de un lock de melee el kiting NO aplica a propósito — el motor no deja
  disparar ahí. Soltarse de un lock es trabajo del zanzoken defensivo.

**6.5 — Que no se quede plantado disparando.** Alejarse mucho (>2200 unidades) con línea
de vista. Esperado: el bot dispara **y avanza**. Si se queda quieto, `engage_neutral_dist`
no se está leyendo.

**6.6 — Skill 1 no debería cambiar nada.** Skill 1 tiene `uses_engage_intent=0`: debe
comportarse exactamente igual que antes. No debería aparecer ninguna línea
`Engage intent`. Si aparece, el cfg no se está aplicando.

**Todos los pesos están en `.cfg`** — no hace falta recompilar para ajustarlos, solo
reiniciar el mapa.
