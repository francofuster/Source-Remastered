# ZEQ2 Remaster para Android

Rama `android` de Source-Remastered. Los datos del juego para Android están en `..\Build-Android\ZEQ2`, y el APK en `..\Build-Android\ZEQ2-debug.apk`.

Propuesta completa: https://claude.ai/artifact/BoERV5dvduJBkzhiXYapQz

## Estado

**F0, cadena de compilación: hecha.** El APK se compila con Gradle y CMake. Incluye el motor (`libmain.so`), SDL 2 (`libSDL2.so`) y los tres módulos nativos (`libcgame.so`, `libgame.so`, `libui.so`). Las librerías están alineadas a 16 KB, con `targetSdk 36` y `minSdk 24`.

**F1, arranque: hecha en el emulador.** Falta el hito en un móvil real: una partida contra un bot con teclado Bluetooth.

- **Datos dentro del APK:** el APK pesa 225 MB y lleva los datos del juego. La primera vez los descomprime (325 MB, unos 6 s en el emulador) y después arranca directamente.
- **Probado en el emulador (Android 15, x86_64):**
  - Partida en `desert` con bots: los bots fijan objetivo, pelean cuerpo a cuerpo y se hacen daño.
  - El teclado mueve al jugador y gira la cámara. La consola acepta texto.
  - El botón atrás abre el menú de pausa, que se maneja con flechas y Enter.
  - Al salir de la app y volver, sigue el mismo proceso y la imagen se sigue moviendo.
  - `quit` cierra la app sin informe de fallo.
- **Memoria:** en partida, 360 MB PSS (275 MB de heap nativo).

**F2, controles táctiles: hecha en el emulador.** Falta el hito en un móvil: una partida completa, del menú a la victoria, sin teclado.

- **Menús:** el dedo es un cursor absoluto más el botón izquierdo. Se tocó, con éxito, PLAY → CREATE → FIGHT!, la pausa y RESUME.
- **Controles en partida** (`Engine/client/cl_touch.c`, medidas de la propuesta):
  - **Stick flotante** en la mitad izquierda. Por debajo del 50 % camina; más allá del 120 % durante 120 ms activa el boost. Con lock-on usa 8 sectores.
  - **Cámara:** se gira arrastrando en la mitad derecha, o arrastrando fuera de KI o FUERTE sin soltar el botón.
  - **Botones:** KI, FUERTE, Z, GUARDIA y LOCK.
  - **AURA:** mantener carga ki; deslizar arriba o abajo cambia de tier.
  - **VUELO:** un toque en ▲ salta, mantener ▲ sube y ▼ baja (aterriza cerca del suelo).
  - **TÉCNICA:** un toque pasa a la técnica siguiente; deslizar arriba o abajo recorre la lista.
  - **Pausa.**
- **Probado en el emulador, con un solo dedo:** stick, cámara, KI, AURA, VUELO, TÉCNICA, la pausa y los menús. Las combinaciones a la vez (stick y botón) necesitan un móvil.
- **Cvars:** `touch_enable` (0 los oculta para jugar con mando o teclado), `touch_alpha`, `touch_scale` (tamaño de todo el diseño) y `touch_lookSpeed` (grados por dp).
- **Gráficos de los botones:** `Android/data/touch/*.png`, generados con `Android/tools/make_touch_art.py`. Gradle los mete en los datos del APK.
- **Pendiente:**
  - Teclado en pantalla para los campos de texto (nombre, IP).
  - Vibración al entrar en boost.
  - Ocultar los controles solos al usar un mando.
  - Contextos, macros, auto-fijado y aim assist: son F3.

## Estructura

| Ruta | Qué es |
|---|---|
| `CMakeLists.txt` | La parte nativa del APK. Sus listas de fuentes replican `Q3OBJ`, `CGOBJ_`, `GOBJ_` y `UIOBJ_` del `Makefile`: un archivo nuevo también hay que añadirlo aquí. |
| `Android/` | Proyecto Gradle: AGP 9.4.1 y wrapper de Gradle 9.8.1. La tarea `packZeqData` comprime `Build-Android/ZEQ2` en `assets/zeq2data.zip`. |
| `.../SetupActivity.java` | Lanzador. Descomprime los datos si cambiaron y abre el juego, pasándole los argumentos `--es args "..."`. |
| `.../ZEQ2Activity.java` | La actividad del juego, que hereda de `SDLActivity`. |
| `Android/third_party/SDL2` | SDL 2.32.10, copiado de `Source-LastRevision/deps/.sdl2`. |
| `Android/third_party/gl4es` | gl4es, commit `bc4314d` del 30-10-2026, sin los tests ni las trazas. Traduce OpenGL 1.x a GLES 2. |
| `Android/third_party/libogg`, `libvorbis` | libogg 1.3.3 y libvorbis 1.3.6, para la música. |
| `Build-Android.ps1` | Compila, instala y lanza el juego. |

## Requisitos (los de este PC)

- Android SDK en `D:\Android`, con platform 36, build-tools 36, NDK `29.0.14206865` y CMake `3.31.6`.
- JDK 27 en `C:\Program Files\Java\jdk-27`.
- Emulador: AVD `zeq2_api35` en `D:\Android\avd`. Antes de lanzarlo, define `ANDROID_AVD_HOME=D:\Android\avd`.

## Uso

```powershell
.\Build-Android.ps1                              # APK arm64, para un móvil
.\Build-Android.ps1 -Emulator                    # también x86_64, para el emulador
.\Build-Android.ps1 -Install -Run                # instalar, abrir y seguir el log
.\Build-Android.ps1 -NoBuild -Run -Map desert    # entrar directo en un mapa
.\Build-Android.ps1 -Clean -Install              # desinstalar antes: borra datos y configuración
```

En el móvil, los datos quedan en `Android/data/io.github.francofuster.zeq2remaster/files/ZEQ2`. El motor usa esa carpeta como `fs_basepath` y como `fs_homepath`, así que la configuración y `qconsole.log` también van ahí. Se pueden ver por USB.

Los datos se descomprimen como archivos sueltos, igual que en PC, y no como pk3. Dentro de un pk3, `trap_FS_GetFileList("players/", "/")` devuelve otra cosa, y la lista de luchadores y los bots dependen de ella.

## Cambios en el código

La parte de PC sigue con SDL 1.2. Todo lo de SDL 2 está dentro de `#if SDL_MAJOR_VERSION >= 2`, y lo propio de Android dentro de `#ifdef __ANDROID__`.

- `Engine/sdl/*`: ventana y contexto GLES con SDL 2, inicialización de gl4es, teclado y sonido. En Android el texto del teclado físico llega sin abrir el teclado de pantalla.
- `Engine/sys/sys_main.c`:
  - Los módulos se cargan como `lib<nombre>.so` desde el APK.
  - SIGSEGV, SIGILL y SIGFPE quedan en manos de debuggerd, para que el backtrace salga en logcat.
  - El botón atrás llega al juego como ESC.
  - Se sale con `_exit`, porque `exit()` ejecutaba los destructores de Android y provocaba un fallo.
- `Engine/sys/sys_unix.c`: rutas de Android y diálogos de error. SIGABRT, SIGBUS y SIGTRAP también se dejan a debuggerd.
- `Engine/sys/con_passive.c`: la consola escribe en logcat, línea a línea y sin códigos de color.
- `Engine/client/cl_touch.c` (con `USE_TOUCH`): los controles táctiles. `sdl_input.c` le pasa los dedos, `CL_CreateCmd` le pide el `usercmd_t` y `SCR_DrawScreenField` lo dibuja.
- `Engine/renderer/tr_backend.c`, `RB_SetViewport`: gl4es recuerda el último viewport y se salta una llamada igual, pero el viewport real de GLES puede ser otro. Tras la vista 3D de un menú, la pantalla siguiente salía encogida en ese hueco. Ahora se envía antes un valor desechable para forzar la llamada real.
- `SetupActivity` le pasa al motor la carpeta de datos (`fs_basepath` y `fs_homepath`). Justo tras arrancar el móvil, Android aún puede no ofrecer el almacenamiento compartido; entonces los datos van a la memoria interna, y el motor los busca ahí.
- `Shared/vm.c`: en Android los módulos siempre son nativos, aunque `default.cfg` pida `vm_* 2`.
- `Shared/q_platform.h`: `ARCH_STRING "arm64"`.

### Fallos del código compartido

Son fallos que el QVM toleraba y que en nativo rompen. **También existen en Remastered (PC).**

**Variables leídas sin inicializar.** El QVM solía encontrar ceros en su pila; en nativo hay basura.
- `Game/Game/bg_pmove.c`, `PM_Friction`: `drop` sin inicializar. En Android congelaba la partida, porque la velocidad y la posición del jugador pasaban a NaN en el primer segundo. En PC el rozamiento dependía de lo que hubiera en la pila.
- `Game/CGame/cg_view.c`, `CG_OffsetFirstPersonView`: `pe`, `tagOrient` y `origin` eran punteros sin asignar. Solo se usa con `cg_thirdPerson 0`.
- `cg_ents.c`, `cg_effects.c`, `cg_music.c` (`nextIndex`), `ui_players.c`, `g_team.c` y `ui_specifyserver.c`: casos menores.

**Otros fallos:**
- `Game/CGame/cg_music.c`: el lector de `music/playlist.cfg` comparaba un `char` suelto con `strcmp` y copiaba las duraciones en un búfer de 8 bytes sin límite. Al entrar en un mapa abortaba por desbordamiento de pila. Además escribía en `lastTrack[-1]` y `temp[-1]`.
- `Game/CGame/cg_weapGfxParser.c`: un `%s` recibía un `int`.
- `Engine/renderer/tr_model.c` y `tr_md4.c`: un offset calculado a mano se truncaba a `int`.
- Prototipos que faltaban: `trap_RealTime` en `bg_local.h`, las funciones de tiers y `G_CheckSkills` en `g_local.h`, `CG_DrawChat` en `cg_local.h` y las pistas de fondo en `ui_local.h`.

La compilación de Android falla ante cualquier truncamiento de puntero, función sin declarar o variable leída sin inicializar (`-Werror=uninitialized`, `-Werror=sometimes-uninitialized`).

## Problemas conocidos

- **Bloom (F4):** con gl4es deja la pantalla en blanco. Está apagado en `Build-Android/ZEQ2/default.cfg` (`r_bloom 0`), que es la única diferencia con el de Remastered.
- **Contornos (F4):** salen como malla de alambre. gl4es emula `glPolygonMode(GL_LINE)` dibujando todas las aristas. La solución es la cáscara invertida.
- **Sombra (F4):** la del jugador es una silueta negra opaca proyectada en el suelo.
- **HUD (F4):** la barra de técnicas queda bajo el stick; la propuesta la sube arriba.
- **Rendimiento:** no se ha medido en un móvil. Las 125 fps del emulador usan la GPU del PC.
- **Memoria:** `com_hunkMegs` es 768 en `default.cfg`. En partida se usan 360 MB, pero no se ha probado en un móvil con poca RAM.
- **Contexto EGL:** al volver a la app, SDL conserva el contexto. Si un móvil lo pierde, falta recargar las texturas (`vid_restart`).
- **Datos antiguos:** si una versión nueva quita archivos de los datos, los antiguos se quedan en el móvil.
- **Sonido (también en PC):** `players/vegeta*/default.grfx` pide `skills/energyBeam/beamCharging.ogg`, pero el archivo se llama `beamCharge.ogg`.
- **Firma:** el APK está firmado con la clave de depuración.
