# Índice del proyecto — ZEQ2-Lite Remaster

> Generado para orientarse rápido sin tener que releer/grepear todo el árbol en cada sesión.
> Motor derivado de ioquake3 (id Tech 3). Dos carpetas de nivel superior en `D:/ZEQ2/Remaster`:
> - `Source-Remastered/` — código fuente (este repo, tiene `.git`)
> - `Build-Remastered/` — binarios compilados + assets del juego (pk3, cfgs, botsys/, etc.)

## Estructura de alto nivel (Source-Remastered)

```
Engine/     motor (cliente, servidor, renderer, botlib de id, libs de terceros)
Game/       QVMs del juego: Game (servidor de juego), CGame (cliente de juego), UI (menús)
Shared/     código compartido motor/juego (qcommon, matemáticas, colisión, vm)
Tools/      herramientas externas de build (compiladores, exporters, etc.)
Makefile    build principal (MinGW/gcc); ver targets client/ded/Base(Game/CGame/UI)
```

- `Engine/botlib/` (64 archivos, `be_aas_*`, `be_ai_*`) — **botlib clásico de id Software (Q3A), NO se compila**. No aparece en el `Makefile` salvo `be_ai_chat.o` (usado solo como utilidad de chat, no de movimiento/IA). Se puede considerar código muerto/histórico salvo que se quiera revivir el sistema AAS de navegación.
- `Game/Game/ai_*.c/h` (dmnet, dmq3, cmd, chat, team, vcmd — id Software) — **tampoco se compilan** (no están en el Makefile). Código legado, no tocar salvo referencia histórica.
- El sistema de bots **activo y compilado** es el propio de ZEQ2: **BotLite** (`g_botlite_*`). Ver sección dedicada abajo.

## Engine/ (motor, tamaño por subcarpeta)

| Carpeta | Archivos | Contenido |
|---|---|---|
| `client/` | 33 | cliente: input, sonido, consola, descargas, etc. |
| `server/` | 11 | lógica de servidor (sv_*.c) |
| `renderer/` | 36 | renderer OpenGL |
| `botlib/` | 64 | **botlib id Q3A — no compilado**, ver arriba |
| `sys/` | 12 | capa de sistema operativo (Win/Linux/Mac) |
| `sdl/`, `SDL12/`, `asm/`, `AL/` | — | input/audio backends |
| `jpeg-6b/`, `zlib/`, `libspeex/`, `libcurl/`, `ogg_vorbis/`, `null/` | — | librerías de terceros vendorizadas |

## Game/Game (servidor de juego — QVM `qagame`)

79 archivos. Núcleo de reglas del juego (Q3-style: `g_*.c`), física/movimiento compartido (`bg_*.c`), armas custom (`g_userweapons*`, `g_weapPhys*`), tiers/rankings específicos de ZEQ2 (`g_tiers.c`, `g_rankings.c`), y el sistema de bots `g_botlite_*` (ver abajo). `g_main.c` es el entry point del QVM (`GAME_*` syscalls), incluye el hook de `BotAIStartFrame` y `BotLite_ResetAll`.

## Game/CGame (cliente de juego — QVM `cgame`)

46 archivos: predicción, dibujo de HUD/menús en juego, partículas, auras (`cg_auras.c` — específico DBZ), trails, cámara, tiers. Sin lógica de bots (todo el lado bot vive en el servidor de juego).

## Game/UI (menú principal — QVM `ui`)

Menús clásicos Q3 adaptados: server browser, player model, login, signup, credits, etc. No hay UI dedicada para configurar bots (se gestionan por consola, ver abajo).

---

## Sistema de Bots — BotLite (lo relevante para tu tarea)

### Ubicación del código
Todo vive en `Game/Game/`, prefijo `g_botlite_*`:

| Archivo | Líneas | Responsabilidad |
|---|---|---|
| `g_botlite.h` | 359 | **Header central**: tipos, enums, `botlite_info_t`, snapshot, vtable de skills, prototipos de toda la API pública |
| `g_botlite_main.c` | 355 | Ciclo de vida: `BotAISetup/Shutdown/LoadMap/SetupClient/StartFrame`, loop principal que llama `BotLite_ThinkClient` por cada bot gestionado |
| `g_botlite_profile.c` | 851 | **El más grande.** Carga de perfiles/archetypes desde disco (`botsys/archetypes/*.cfg`), resolución de personaje/skill, `BotLite_AddBot`/`AddBotDebug`/`RemoveBot*` |
| `g_botlite_ai.c` | 335 | `BotLite_ThinkClient` (entrada por-frame de cada bot), construcción del snapshot de percepción, reacciones a daño |
| `g_botlite_skill.c` | 400 | Define las 3 vtables de skill (`botlite_skill1/2/3_vtable`) y `BotLite_GetSkillVTable(skill)` — despacho polimórfico según nivel de dificultad |
| `g_botlite_skill1.c` / `skill2.c` / `skill3.c` | 10-14 c/u | Wrappers finos que delegan en la lógica genérica o en tácticas específicas por nivel (skill3 tiene más comportamiento propio: heal, block/dodge, power melee) |
| `g_botlite_target.c` | 542 | Selección y validación de objetivo (`BotLite_TargetIsValid`, recovery state del target, snapshot metrics) |
| `g_botlite_movement.c` | 617 | Movimiento: patrones de búsqueda, approach, retreat, post-crash flyup |
| `g_botlite_melee.c` | 522 | Combos cuerpo a cuerpo (punch/kick/speed/finish), estados de combate cercano |
| `g_botlite_ranged.c` | 214 | Ataques a distancia, cambio de arma, ventanas de carga |
| `g_botlite_tactics.c` | 10 | Selección/despacho de táctica (wrapper) |
| `g_botlite_lockon.c` | 60 | Lock-on de cámara/mira hacia el objetivo (`FaceTarget`, `SetLockOn`) |
| `g_botlite_action.c` | 124 | Traducción de decisiones a input simulado (`BotLite_EA_MoveForward/Back/Right/Up`, botones, arma) — equivalente al "elementary actions" de id pero propio |

Total ≈ 5.360 líneas.

### Arquitectura (resumen mental)

1. **Comandos de consola** (`Game/Game/g_cmds.c` ~línea 29-120): `/addbot <character> [skill 1-3] [-debug]`, `/removebot <character>` → llaman a `BotLite_AddBot`/`AddBotDebug`/`RemoveBotByName` en `g_botlite_profile.c`.
2. **Estado por bot**: array global `g_botlite[BOTLITE_MAX_BOTS]` de `botlite_info_t` (indexado por clientNum), definido en `g_botlite.h:262`. Contiene perfil numérico (agresión, rush, block, ranged bias...), puntero a `botlite_profile_t` (distancias/tiempos tuneables) y a la `botlite_skill_vtable_t` correspondiente, más sub-estados: `runtime`, `search`, `melee`, `ranged`, `recovery`, `action`.
3. **Frame loop**: `BotAIStartFrame(time)` (llamado desde `g_main.c`) recorre clientes gestionados (`BotLite_IsManagedBot`) y llama `BotLite_ThinkClient(clientNum, time)` en `g_botlite_ai.c`.
4. **Percepción → snapshot**: cada think arma un `botlite_snapshot_t` (distancia, LOS, si el target está en combo/bloqueando/cargando/crasheado/recovering, etc.).
5. **Decisión polimórfica por skill**: vía `botlite_skill_vtable_t` (`ResetRuntime`, `RunSearch`, `SelectCombatTactic`, `RunCombatTactic`), resuelta con `BotLite_GetSkillVTable(skill)` según el nivel 1/2/3 del bot.
6. **Tácticas** (`botlite_tactic_t`): SEARCH_PATTERN, APPROACH, MELEE_PRESSURE, RANGED_PRESSURE, PUNISH_RECOVERY, RETREAT_RECOVERY, REPOSITION, IDLE_TRACK — implementadas repartidas entre `g_botlite_movement.c`, `_melee.c`, `_ranged.c`, `_target.c`.
7. **Ejecución de acción**: la táctica termina escribiendo un `botlite_action_t` (forward/right/up/buttons/weapon) vía las funciones `BotLite_EA_*` en `g_botlite_action.c`, que se comittea al `usercmd_t` real del bot (`BotLite_ActionCommit`).

### Configuración basada en datos (fuera del código, en Build-Remastered)

`Build-Remastered/ZEQ2/botsys/`:
- `archetypes/*.cfg` (aggressive, balanced, bruiser, custom, keepaway, mobile, tactical) — perfiles de personalidad/tácticas que `BotLite_BotsysApplyInfoConfig` carga en runtime (función en `g_botlite_profile.c:799`).
- `skills/skill1.cfg` / `skill2.cfg` / `skill3.cfg` / `defaults.cfg` — tuning numérico por nivel de dificultad.
- `bots/bots.txt` — lista/registro de bots disponibles.
- Cada subcarpeta trae `README_EN.txt` / `README_ES.txt` explicando el formato — **léelos antes de tocar el formato de .cfg**, son la fuente de verdad del formato de archivo, no está hardcodeado en un parser genérico documentado en el header.

### Puntos de entrada útiles según lo que quieras hacer

- **Añadir/tunear una táctica de combate** → `g_botlite_melee.c` / `g_botlite_ranged.c` / `g_botlite_tactics.c`, registrar en la vtable correspondiente en `g_botlite_skill.c`.
- **Cambiar cómo se elige objetivo** → `g_botlite_target.c`.
- **Nuevo nivel de dificultad o comportamiento por skill** → `g_botlite_skill.c` (vtable) + nuevo `g_botlite_skillN.c` si aplica.
- **Nueva personalidad/archetype sin tocar C** → solo añadir `.cfg` en `botsys/archetypes/` (revisar README del formato primero).
- **Ciclo de vida (spawn/despawn/reset en respawn o cambio de mapa)** → `g_botlite_main.c` y `g_botlite_profile.c`.
- **Comandos de consola nuevos** → `g_cmds.c` (buscar `Cmd_AddBot_f` / `Cmd_RemoveBot_f` como plantilla).

### Nota importante
No confundir con `Engine/botlib/` ni `Game/Game/ai_*.c` — ambos son el sistema original de id Software y **están excluidos del build**. Cualquier trabajo de "implementar bots" en este proyecto debe hacerse sobre **BotLite**, no sobre esos archivos legado.
