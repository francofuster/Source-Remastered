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
  - Vibración al entrar en boost.
  - Ocultar los controles solos al usar un mando.

**F3, contextos y asistencias: hecha en el emulador.** Falta probarla en un móvil, con dos dedos a la vez.

- **Contextos** (`cl_touch.c`, `Touch_UpdateContext` y `Touch_Label`): los botones cambian de texto según la situación, leída del `playerState` de la instantánea.
  - Cuerpo a cuerpo: KI pasa a COMBO, FUERTE a LANZAR y Z a CONTRA.
  - Rival lanzado: Z pasa a PERSEGUIR y parpadea; KI, FUERTE y GUARDIA se atenúan.
  - También ESQUIVAR (un rival a menos de 200 unidades que se acerca rápido), ROMPER, ATRÁS, FRENAR, RECUPERAR y EMPUJAR (choque de rayos).
- **Combo guiado** (perfil Asistido, `touch_profile 1`): en cuerpo a cuerpo, mantener KI encadena golpes, una ráfaga y un lanzamiento; un toque da un golpe. Mantener FUERTE y mover el stick elige hacia dónde se lanza. El perfil Clásico (`touch_profile 0`) deja solo la fricción.
- **LOCK:** gira la vista hacia el rival visible más cercano a la mira (primero dentro de 22°; si no hay ninguno, en cualquier dirección) y fija en el comando siguiente. El juego traza el fijado antes de aplicar los ángulos del comando (`PM_CheckLockon` va antes que `PM_UpdateViewAngles`), por eso el botón sale un comando después del giro.
- **Aim assist** (`Engine/client/cl_aimassist.c`, solo sin lock-on):
  - Captura 22°; fricción a 11° (la cámara gira al 55 %); imán a 5° (28°/s); sigue al rival si se mueve.
  - El disparo se corrige hasta 2,5° hacia el rival.
  - Auto-fijado tras 400 ms atacando con el rival en el imán.
  - Mide desde la vista real, `cl.viewangles` más `delta_angles`: el aparecer, `setviewpos` y el lock-on mueven `delta_angles`.
  - El servidor puede prohibirlo con `sv_aimAssist 0`. Cvars `cl_aimAssist` y `cl_aa*`; `cl_aaDebug 1` traza al pulsar LOCK y `cl_aaDebug 2` en cada frame.
- **HUD:** en Android va arriba y un 25 % más grande (`cg_hudTop 1`, `cg_hudScale 1.25`; en PC, 0 y 1), como el del lock-on. La barra de técnicas va debajo y el stick ya no lo tapa.
- **Probado en el emulador:** LOCK con el rival a la espalda, acercarse, entrar en cuerpo a cuerpo, el combo guiado hasta lanzarlo, PERSEGUIR y la vuelta a los textos normales. Sin probar: dos dedos a la vez, ESQUIVAR, el choque de rayos y el auto-fijado al disparar.
- **Pruebas por adb:** las teclas inyectadas no abren la consola de forma fiable, y el texto acaba en el chat. Para mandar comandos, arrancar con `+bind F6 exec t.cfg`, subir `t.cfg` con `adb push` a la carpeta de datos y pulsar `adb shell input keyevent F6`. Con `devmap`, `cl_aaDebug 2` da la posición del bot para un `setviewpos`.

**F4, render móvil: hecha en el emulador.** Falta el hito en un móvil de gama media: 60 fps, y 30 como mínimo, en `lastStand` con dos luchadores y auras (`cg_drawFPS 1` muestra las fps). En el emulador no sirve medir: usa la GPU del PC.

- **Resolución interna:** el juego dibuja a `r_androidHeight` píxeles de alto (720 por defecto; 0 = la del móvil) y el sistema lo escala a la pantalla, sin coste. En un móvil de 2400 × 1080 son 1600 × 720, menos de la mitad de píxeles.
  - `ZEQ2Surface` crea la superficie ya reducida (`setFixedSize` al medir la vista) y sigue dividiendo los toques por el tamaño de la vista, no por el del búfer.
  - `ZEQ2Activity` lee el valor de `zeq2config.cfg` antes de arrancar el motor, así que un cambio vale al volver a abrir el juego. Para pruebas, `+set r_androidHeight N` en los argumentos de adb tiene prioridad.
  - `IN_DisplayDPI` da la densidad en píxeles de la ventana, para que los controles táctiles midan lo mismo en la pantalla.
  - Probado en el emulador a 720 y a 0: los controles quedan en el mismo sitio y con el mismo tamaño, los toques aciertan en partida y en los menús, y la imagen solo se ve algo más suave.
- **Efectos que gl4es no puede hacer**, apagados en el código aunque se activen en las opciones: bloom y motion blur (copian el búfer de pantalla a una textura: sale en blanco y es caro en las GPU de móvil) y los destellos de luces (`r_flares`, necesitan leer el búfer de profundidad, que GLES no permite).
- **`Build-Android/ZEQ2/default.cfg`** para móvil: `com_maxfps 60`, `r_flares 0`, `r_detailtextures 0`, `r_bloom 0` y `r_androidHeight 720`. Solo cuenta en una instalación nueva: en una existente manda `zeq2config.cfg`.
- **Sombras:** las de los luchadores (`cg_shadows 3`) son siluetas negras proyectadas en el suelo. Es el mismo shader que en PC (`projectionShadow` en `scripts/common.shader`, negro con `blendfunc filter`), así que no es un fallo de gl4es; se dejan como en PC.
- **Si va lento en el móvil**, por orden: bajar `r_androidHeight` (600 o 540), `r_dynamiclight 0` (cada luz de un ataque vuelve a dibujar lo que ilumina), `cg_particlesQuality 0` (sin restos en las explosiones) y `cg_shadows 1` (una mancha en vez de la silueta).

**Menús táctiles: hecha en el emulador.** Propuesta visual: https://claude.ai/artifact/UbfGFz7USXg42NbkdaZbGH. Falta probarla en un móvil.

- **Cómo se elige el diseño:** `ui_layout` (0 según la pantalla, 1 PC, 2 móvil). En 0 es móvil si el motor tiene capa táctil y la pantalla mide menos de 600 dp de alto (`in_screenDp`, que escribe el motor); una tableta se queda con el de PC. Cada pantalla lo lee al abrirse. Con el de PC, el juego se ve y se maneja como antes.
- **Lienzo móvil:** las pantallas convertidas se dibujan sobre 844 × 390 (un píxel de diseño es más o menos un dp) en vez de 1280 × 720. Cada una tiene su tabla de posiciones para PC y para móvil, y marca su menú con `menu.mobile`; `UI_DFit` usa el lienzo del menú activo. Al pulsar algo, el lienzo vuelve al de PC antes de abrir otra pantalla, para que una no convertida no herede el móvil.
- **Cómo se toca** (`ui_atoms.c`, con `in_inputMode 1`, que escribe el motor: 0 ratón o teclado, 1 dedo, 2 mando):
  - Un botón solo se ilumina mientras el dedo está encima, y se activa al levantarlo sobre el mismo botón.
  - Si el dedo se mueve más de unos 10 dp, no activa nada: el arrastre vertical desplaza la lista (pasos de rueda; también la rueda del ratón en PC).
  - Los deslizadores siguen al dedo desde que se apoya.
  - Sin cursor, sin foco fijo y sin la barra de teclas. El foco con teclado o mando sigue como antes.
- **Pantallas convertidas:**
  - Principal: botones de 58 dp con subtítulo; la placa del nombre abre FIGHTER; Atrás pregunta «Quit?» (también en PC: antes el motor volvía a abrir el menú).
  - Pausa: RESUME y una rejilla de 2 × 3; LEAVE MATCH pregunta antes (también en PC).
  - Pausa › Bots: filas de 60 dp que se desplazan en vez de encoger, × de 52 dp, congelar de 52 dp; «Kick all» pregunta (también en PC).
  - Jugar: pestañas de 48 dp con una ADDRESS para conectar por dirección, filas de 58 dp (tocar elige, tocar otra vez entra), REFRESH, JOIN y el orden en botones.
  - Crear partida: tarjeta del mapa con flechas, reglas de 56 dp con − / + (pasos de 1, 5 o 500) que se desplazan, luchadores en retratos de 52 dp; sin «Dedicated».
  - Luchadores (MANAGE) y Añadir o editar bot: filas de 60 dp con ✎ y ×; personajes, nivel y estilo en piezas de 44-58 dp; «Remove all» pregunta (también en PC).
  - Luchador: rejilla de 4 × 2 por páginas (tocar el contador o arrastrar), skins y transformaciones de 48 dp; la pista «F + W» pasa a «hold AURA and swipe up».
  - Opciones (VIDEO & AUDIO, GAME) y CAMERA: filas de 56 dp con la ayuda debajo, una ventana que se desplaza, deslizadores en toda la pista y flechas de selector que retroceden o avanzan. En móvil desaparecen resolución, aspecto, color y ventana.
  - CONTROLS pasa a **TOUCH CONTROLS** (`ui_touch.c`, nuevo): perfil, aim assist, velocidad de la mirada, tamaño y opacidad de los botones, altura de render, RESET y una vista previa con los botones donde los pone `cl_touch.c`.
  - Diálogos: YES y NO de 52 dp; tocar fuera del diálogo es NO.
- **Teclado de pantalla:** tocar un campo de texto (nombre, dirección, nombre de servidor o de bot) lo abre (`in_textInput`, que el motor convierte en `SDL_StartTextInput`). Mientras está abierto, una barra arriba enseña el texto con DONE; se cierra con DONE, con Intro, cambiando de campo o de pantalla, o con Atrás (el motor lo detecta y avisa).
  - `ZEQ2Surface` mide siempre todo el espacio: con el búfer reducido, la vista de texto de SDL encogía el juego a una esquina y perdía los toques.
  - La actividad usa `windowSoftInputMode="adjustNothing"`: el teclado se superpone y no redimensiona el juego.
- **Sin convertir:** la conexión y la carga (solo texto; Atrás cancela), los créditos y las pantallas antiguas que ya no se usan. Siguen con el lienzo de PC y se pueden tocar.
- **Pendiente:** probar en un móvil (tamaños reales, el teclado), la lista de órdenes del navegador (hoy un toque pasa al siguiente) y las direcciones recientes de ADDRESS.

## Estructura

| Ruta | Qué es |
|---|---|
| `CMakeLists.txt` | La parte nativa del APK. Sus listas de fuentes replican `Q3OBJ`, `CGOBJ_`, `GOBJ_` y `UIOBJ_` del `Makefile`: un archivo nuevo también hay que añadirlo aquí. |
| `Android/` | Proyecto Gradle: AGP 9.4.1 y wrapper de Gradle 9.8.1. La tarea `packZeqData` comprime `Build-Android/ZEQ2` en `assets/zeq2data.zip`. |
| `.../SetupActivity.java` | Lanzador. Descomprime los datos si cambiaron y abre el juego, pasándole los argumentos `--es args "..."`. |
| `.../ZEQ2Activity.java` | La actividad del juego, que hereda de `SDLActivity`. |
| `.../ZEQ2Surface.java` | La superficie del juego: dibuja a `r_androidHeight` y escala a la pantalla. |
| `Game/UI/ui_touch.c` | La página TOUCH CONTROLS de las opciones (diseño móvil). |
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
- `Engine/client/cl_aimassist.c` (con `USE_TOUCH`): el aim assist. `CL_TouchMove` lo llama al final, y `CL_CreateCmd` llama a `CL_AimAssistShot` tras `CL_FinishMove`.
- `Engine/server/sv_init.c`: `sv_aimAssist`, en la información del sistema.
- `Game/CGame/cg_draw.c` y `cg_weapons.c`: `cg_hudTop` y `cg_hudScale`. En PC no cambian nada.
- `Engine/renderer/tr_backend.c`, `RB_SetViewport`: gl4es recuerda el último viewport y se salta una llamada igual, pero el viewport real de GLES puede ser otro. Tras la vista 3D de un menú, la pantalla siguiente salía encogida en ese hueco. Ahora se envía antes un valor desechable para forzar la llamada real.
- `Engine/renderer/tr_shade.c`, `RB_OutlinesPass`: GLES no tiene `glPolygonMode`, y gl4es convierte los triángulos en líneas que el culling no descarta, así que se veía la malla entera del personaje. Con gl4es, el contorno es una cáscara invertida: las caras traseras se desplazan hacia fuera por su normal, en el color del contorno, y solo asoma un borde en la silueta. El desplazamiento crece con la distancia, para que el borde mida en píxeles lo mismo que las líneas de PC.
- `Engine/renderer/tr_bloom.c`, `tr_motionblur.c` y `tr_flares.c`: con gl4es salen sin dibujar nada.
- `Engine/sdl/sdl_glimp.c`: registra `r_androidHeight`; `sdl_input.c`, `IN_DisplayDPI`: la densidad en píxeles de la ventana.
- `Engine/sdl/sdl_input.c`, para los menús: `in_inputMode` (lo último usado: ratón o teclado, dedo o mando), `in_screenDp` (alto de la pantalla en dp) y el puente del teclado de pantalla (`in_textInput`).
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

**Punteros del lock-on.** `Pmove` lee `ps->lockedPlayer` y `ps->lockedPosition` en cuanto hay `lockedTarget`. El servidor solo los ponía en `EV_MELEE_CHECK`, cada 300 ms, y no viajan en la instantánea, así que en el cliente siempre eran NULL. El QVM leía basura en la dirección 0; en nativo, fijar a alguien cerraba el juego.
- `Game/Game/g_active.c`, `ClientThink_real`: se apuntan al rival antes de cada `Pmove`. Un choque de rayos, que pone en `lockedTarget` el número del misil, conserva su puntero.
- `Game/CGame/cg_predict.c`, `CG_PredictLockedPointers`: la predicción los apunta a una copia con lo que el cliente sabe del rival.
- `Game/Game/bg_pmove.c`: el frame en que se toma el fijado aún no los tiene. `PmoveSingle`, `PM_CheckLockon`, `PM_StopLockon`, `PM_EndDrift`, `PM_Melee` y `PM_Weapon` lo comprueban antes de leerlos.

La compilación de Android falla ante cualquier truncamiento de puntero, función sin declarar o variable leída sin inicializar (`-Werror=uninitialized`, `-Werror=sometimes-uninitialized`).

## Problemas conocidos

- **Bloom, motion blur y destellos:** apagados con gl4es (ver F4). Para recuperarlos habría que rehacer el bloom sobre un framebuffer object.
- **Rendimiento:** no se ha medido en un móvil. Las 125 fps del emulador usan la GPU del PC.
- **Memoria:** `com_hunkMegs` es 768 en `default.cfg`. En partida se usan 360 MB, pero no se ha probado en un móvil con poca RAM.
- **Contexto EGL:** al volver a la app, SDL conserva el contexto. Si un móvil lo pierde, falta recargar las texturas (`vid_restart`).
- **Datos antiguos:** si una versión nueva quita archivos de los datos, los antiguos se quedan en el móvil.
- **Sonido (también en PC):** `players/vegeta*/default.grfx` pide `skills/energyBeam/beamCharging.ogg`, pero el archivo se llama `beamCharge.ogg`.
- **Firma:** el APK está firmado con la clave de depuración.
