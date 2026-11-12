# ZEQ2-Lite Mobile (ZEQ2 Remaster para Android)

Rama `android` de Source-Remastered. Los datos del juego para Android están en `..\Build-Android\ZEQ2`; el APK de pruebas en `..\Build-Android\ZEQ2-debug.apk` y el de release en `..\Build-Android\ZEQ2-<versión>.apk`. En el móvil la app se llama **ZEQ2-Lite Mobile**.

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
  - **Cámara:** se gira arrastrando en la mitad derecha, o arrastrando fuera de KI sin soltar el botón.
  - **Botones:** KI, Z, GUARDIA, LOCK, AURA, TÉCNICA y VUELO. FUERTE ya no es un botón: es KI deslizado a la izquierda.
  - **KI:** al tocarlo espera hasta 200 ms sin mandar nada. Si en ese tiempo se desliza 16 dp a la izquierda, es el segundo ataque (MOUSE2, FUERTE, o LANZAR, RECUPERAR o VOLVER según el momento); si no, es KI. Esperar hace falta porque un ataque que no se carga sale en cuanto se aprieta su botón. Un toque corto sigue siendo KI. Bajo la palabra KI se ve la del segundo ataque con una flecha ◀; al deslizar, el botón dice esa palabra y toma el rojo de FUERTE.
  - **Sitios:** Z donde estaba FUERTE, TÉCNICA donde estaba Z. Sin lock-on, LOCK está donde aparece GUARDIA; con lock-on, GUARDIA ocupa ese sitio y LOCK sube, debajo del HUD del rival.
  - **AURA:** es F + WASD bajo un pulgar. Mantener carga ki (F + D); deslizar a la izquierda y mantener baja el ki (F + A), y al volver al centro carga otra vez; deslizar arriba o abajo cambia de tier (F + W, F + S). Mientras se mantiene, alrededor aparecen TRANSFORM, LOWER KI y REVERT, y se ilumina el que está en uso; el aro se vuelve dorado al cargar y verde agua al bajar.
  - **VUELO:** en el suelo, un toque en ▲ salta y mantenerlo despega; en el aire ▲ solo sube (la R de PC). ▼ baja y aterriza cerca del suelo.
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
- **HUD:** en Android va arriba y un 45 % más grande (`cg_hudTop 1`, `cg_hudScale 1.45`; en PC, 0 y 1), como el del lock-on. El del rival usa la misma escala. Las instalaciones que guardaron 1,25 pasan a 1,45 una sola vez (`cg_hudScaleVersion`). La barra de técnicas va debajo y el stick ya no lo tapa.
- **Probado en el emulador:** LOCK con el rival a la espalda, acercarse, entrar en cuerpo a cuerpo, el combo guiado hasta lanzarlo, PERSEGUIR y la vuelta a los textos normales. Sin probar: dos dedos a la vez, ESQUIVAR, el choque de rayos y el auto-fijado al disparar.
- **Pruebas por adb:** las teclas inyectadas no abren la consola de forma fiable, y el texto acaba en el chat. Para mandar comandos, arrancar con `+bind F6 exec t.cfg`, subir `t.cfg` con `adb push` a la carpeta de datos y pulsar `adb shell input keyevent F6`. Con `devmap`, `cl_aaDebug 2` da la posición del bot para un `setviewpos`.

**F4, render móvil: hecha.** Hito cumplido en un Moto G06 (Helio G85, Mali-G52, 1640 × 720, Android 15): 60 fps estables en `lastStand` (16,6 ms por frame, percentil 90 en 16,7 ms), también con bots peleando, medido con `dumpsys SurfaceFlinger --latency`.

- **Resolución interna:** el juego dibuja a `r_androidHeight` píxeles de alto (720 por defecto; 0 = la del móvil) y el sistema lo escala a la pantalla, sin coste. En un móvil de 2400 × 1080 son 1600 × 720, menos de la mitad de píxeles.
  - `ZEQ2Surface` crea la superficie ya reducida (`setFixedSize` al medir la vista) y sigue dividiendo los toques por el tamaño de la vista, no por el del búfer.
  - `ZEQ2Activity` lee el valor de `zeq2config.cfg` antes de arrancar el motor, así que un cambio vale al volver a abrir el juego. Para pruebas, `+set r_androidHeight N` en los argumentos de adb tiene prioridad.
  - `IN_DisplayDPI` da la densidad en píxeles de la ventana, para que los controles táctiles midan lo mismo en la pantalla.
  - Probado en el emulador a 720 y a 0: los controles quedan en el mismo sitio y con el mismo tamaño, los toques aciertan en partida y en los menús, y la imagen solo se ve algo más suave.
- **Efectos que gl4es no puede hacer**, apagados en el código aunque se activen en las opciones: bloom y motion blur (copian el búfer de pantalla a una textura: sale en blanco y es caro en las GPU de móvil) y los destellos de luces (`r_flares`, necesitan leer el búfer de profundidad, que GLES no permite).
- **`Build-Android/ZEQ2/default.cfg`** para móvil: `com_maxfps 60`, `r_flares 0`, `r_detailtextures 0`, `r_bloom 0` y `r_androidHeight 720`. Solo cuenta en una instalación nueva: en una existente manda `zeq2config.cfg`.
- **Sombras:** las de los luchadores (`cg_shadows 3`) son siluetas negras proyectadas en el suelo. Es el mismo shader que en PC (`projectionShadow` en `scripts/common.shader`, negro con `blendfunc filter`), así que no es un fallo de gl4es; se dejan como en PC.
- **Si va lento en el móvil**, por orden: bajar `r_androidHeight` (600 o 540), `r_dynamiclight 0` (cada luz de un ataque vuelve a dibujar lo que ilumina), `cg_particlesQuality 0` (sin restos en las explosiones) y `cg_shadows 1` (una mancha en vez de la silueta).

**Menús táctiles: hecha.** Probada en el emulador y en el Moto G06. Propuesta visual: https://claude.ai/artifact/UbfGFz7USXg42NbkdaZbGH.

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

**F5, paquete: hecha.** Falta el hito: una instalación limpia en tres móviles distintos (hecho en uno, el Moto G06).

- **Primera ejecución** (`SetupActivity`):
  - Antes de descomprimir comprueba el espacio libre: lo que falta de los datos más 64 MB. Si no hay, dice cuántos MB hacen falta y cuántos hay.
  - El progreso se ve en MB.
  - Si la carpeta de `Android/data` no se puede escribir, descomprime en el almacenamiento interno. Pasa cuando una desinstalación deja la carpeta vieja, con el usuario de la instalación anterior (lo vimos en el emulador con `adb uninstall`).
  - Guarda la lista de lo que descomprime (`.zeq2data.files`; desde la 1.4-alpha3, `.zeq2data.index` con el CRC32 de cada archivo). La siguiente actualización borra lo que ya no trae, sin tocar lo que escribe el juego.
  - En el Moto G06 descomprimir los 325 MB tarda unos 21 s; una actualización con los mismos datos no descomprime nada.
- **Versión:** `zeqVersionCode` y `zeqVersionName` en `Android/gradle.properties`. El código tiene que subir con cada APK que se reparta, o Android no lo instala encima.
- **Release:** `.\Build-Android.ps1 -Release` compila el nativo optimizado (RelWithDebInfo), sin depurador, y deja `..\Build-Android\ZEQ2-Lite-Mobile-<versión>.apk` (desde la 1.4-alpha3 sin los datos: ver «Actualizaciones desde la app»).
- **Firma:** la clave de release está en `Android/zeq2-release.jks` y sus datos en `Android/keystore.properties`; los dos quedan fuera de git. **Hay que guardar copia de ambos:** sin esa clave no se puede publicar una actualización que se instale encima. Sin `keystore.properties`, el release sale con la clave de depuración y `-debugkey` en el nombre.
- **Nombre e icono:** «ZEQ2-Lite Mobile». El icono sale de `Android/tools/zeq2icon.png` con `Android/tools/make_icon.py`: adaptativo en Android 8+ (cada launcher lo recorta con su forma) y cuadrado en los anteriores.
- **Instalar en un móvil por Wi-Fi:** activar la depuración inalámbrica, `adb pair <ip:puerto> <código>` con el diálogo «Vincular con código» abierto, y luego `adb connect <ip:puerto>`. Con varios dispositivos conectados: `.\Build-Android.ps1 -Install -Device <serie>`.
- **Una APK firmada con otra clave** (las de depuración de antes) no se instala encima: hay que desinstalar, y eso borra los datos descomprimidos y la configuración.

**Cambios tras probar en el móvil:**
- LOCK más abajo, para no taparse con el HUD del rival.
- Los textos de los botones, en inglés: STRONG, GUARD, SKILL, COMBO, LAUNCH, COUNTER, CHASE, DODGE, RETURN, RECOVER, PUSH.
- GUARD solo aparece con un rival fijado.
- AURA con el stick: mantener AURA y empujar el stick hacia delante transforma (F + W); hacia atrás, destransforma (F + S). Sin stick carga, y deslizar sobre AURA sigue funcionando.
- Crear partida: el poder (y el poder máximo) no pasa de 32767, el máximo del juego.
- Memoria: en una instalación limpia `lastStand` no cargaba («Z_Malloc: failed on allocation of 10240040 bytes from the main zone»): sus texturas de 1600 × 1600 piden un bloque de 10 MB. En Android `com_zoneMegs` vale 64 (`Shared/common.c` y `default.cfg`).

**1.4-alpha2 (después de 1.4-alpha1):**
- **Servidores clásicos** (ZEQ2-Lite original, sin el sistema de pelea de Remastered). Propuesta: https://claude.ai/artifact/FcCxDd91CkQFxpguVLqdVT.
  - Detección: los servidores Remastered publican `g_fightSystem 1` (nuevo, en `g_main.c`) o `g_botStamina` (1.2 a 1.4); sin ninguno, es clásico (`BG_REMASTERED_SERVER` en `bg_public.h`).
  - Z dice siempre Z y no pulsa; en cuerpo a cuerpo y en knockback se ve apagado, porque el original no permite zanzoken ahí. La predicción no corre el zanzoken fijado (`pmove_t.noLockedZanzoken`). Al entrar, 5 s de aviso: «Classic server: Z does not dodge, counter or chase».
  - **Potencia desplazada:** la versión publicada en 2011 (`Build-1536`, gamedate Mar 18 2011) no tiene `plHealthPool` ni `plMaximumPool`, así que todo lo que va después de `plHealth` llegaba dos puestos corrido: el máximo leía el límite (32767) y la barra de vida se veía casi vacía. `CG_ClassicPowerLevels` (`cg_snapshot.c`) lo detecta en el primer snapshot (un servidor actual nunca manda límite 0) y lo devuelve a su sitio. `cg_debugPower 1` imprime lo que llega.
  - La asistencia de apuntado sigue activa en esos servidores (decisión del usuario).
  - Prueba: `Build-1536\ZEQ2Dedicated.exe +set dedicated 1 +set com_zoneMegs 64 +set sv_pure 0 +set net_port 27961 +devmap lastStand` y un cliente original minimizado como rival (el original no tiene bots ni `addbot`); el móvil entra con `+connect <ip del PC>:27961`. El original también necesita `com_zoneMegs 64` para `lastStand`.
- **Cierre por la deriva del cuerpo a cuerpo:** `PM_CmdScale` leía `lockedPlayer->timers[tmMeleeIdle]` sin comprobar el puntero mientras había deriva (`PW_DRIFTING`). Desde que soltar el lock-on borra `lockedPlayer`, una deriva que seguía después del lock-on cerraba el juego (la predicción del cliente ya lo dejaba en NULL sin lock-on). Ahora lo comprueba, y soltar el lock-on termina la deriva de los dos luchadores (`PM_EndDrift`): antes seguía empujando al luchador, a velocidad de deriva. Se revisaron las 83 lecturas por `lockedPlayer` de `bg_pmove.c`: el resto ya estaba protegido.
- Con knockback, KI solo recupera (RECUPERAR, MOUSE2) y lo hace al tocarlo, sin esperar un deslizamiento; mantenido sirve en cuanto el juego deja (pasado el primer segundo del knockback, `tmKnockback` < 4000). FRENAR (MOUSE1) solo reducía a la mitad la velocidad del vuelo y casi no se notaba: se quitó. Si KI ya estaba apretado cuando llega el golpe, también pasa a recuperar. Con `touch_debug 1` la consola dice cómo termina cada knockback: `1 -> -1` es una recuperación (el temporizador salta a -500), `1 -> 0` es que terminó solo.
- **Cierre al fijar otra vez (también latente en PC):** `PM_StopLockon` borraba `lockedTarget` y `lockedPosition` pero dejaba `lockedPlayer` apuntando al rival anterior. Al fijar de nuevo, `PM_CheckLockon` ponía el nuevo `lockedTarget` y, en ese mismo frame, `PM_Melee` pasaba el control con ese puntero viejo y leía `lockedPosition`, que seguía en NULL. Fijar, soltar y fijar cerraba el juego siempre. Ahora soltar y fijar borran los tres; `PM_Melee` exige los dos punteros, la vista de lock-on mira el puntero y no `*ps->lockedPosition`, y el bloqueo de un ataque (`g_usermissile.c`) tampoco deja un `lockedPlayer` viejo.
- HUD más grande (1,45), también el del rival.
- KI deslizado a la izquierda dispara el segundo ataque; FUERTE deja de ser un botón. Z pasa a su sitio, TÉCNICA al de Z, y LOCK al de GUARDIA mientras no hay lock-on.
- Pantalla de carga: el panel de consejos es más grande en el móvil y los consejos hablan de los controles táctiles; sin el marco azul alrededor del icono del jugador.
- VUELO ▲ en el aire: al soltarlo enviaba un salto, que en el aire es el Espacio de PC (planear), y el luchador daba un tirón hacia delante. Ahora en el aire solo sube, y soltarlo salta solo tras un toque corto.
- Bajar el ki (F + A) sin otro botón: mantener AURA y deslizar a la izquierda. Propuesta: https://claude.ai/artifact/2Mh63vvNbfd8ey5oUkem1W.
- Deslizar AURA arriba o abajo casi nunca transformaba. El servidor mira las teclas de tier cada 300 ms (`PM_CheckTransform` → `EV_TIERCHECK` → `checkTier`), y un deslizamiento rápido las mandaba menos tiempo. Uno lento, además, ya había pasado a cargar y se ignoraba. Ahora cuenta a cualquier velocidad, también después de empezar a cargar, y el F + W o F + S se sigue mandando 450 ms aunque se suelte AURA (como los bots, `BOTLITE_TIER_HOLD_MS`). AURA con el stick también lo mantiene 450 ms como mínimo.
- Menú principal en el móvil: la placa del nombre mantiene la proporción de su imagen y ya no lleva «Change fighter».

**32 bits (después de 1.4-alpha2):**
- La APK lleva también `armeabi-v7a` (ARM de 32 bits, muchos móviles que salieron con Android 7 a 9), junto a `arm64-v8a`. Es una sola APK: Android instala la parte que le toca al móvil. Crece 3,7 MB; casi todo su tamaño son los datos, que comparten.
- **Memoria en 32 bits:** `default.cfg` pide un hunk de 768 MB (`com_hunkMegs`) en un solo bloque. Un proceso de 32 bits en Android no lo consigue (512 tampoco, en Android 7) y el juego se cerraba al arrancar con «Hunk data failed to allocate 768 megs». En 32 bits el hunk se limita a 256 MB (`Com_InitHunkMemory`, solo con `!__LP64__`; los 64 bits siguen igual). Medido en el emulador con Android 7: `lastStand` con cuatro luchadores usa 108 MB y `desert`, el mapa más grande, 131 MB.
- Prueba: el emulador de Android 7 x86 (`zeq2_api24_x86`, ver Requisitos), 4 min en `desert` con tres bots, sin cierres; el stick y los botones responden. El Moto G06 solo ejecuta 64 bits (`ro.product.cpu.abilist` = `arm64-v8a`, sin `app_process32`), así que el código ARM de 32 bits no se pudo probar en un móvil real.

**Actualizaciones desde la app (1.4-alpha3).** Propuesta: https://claude.ai/artifact/3taj6BZ3cmGWuxVLSVyStv.
- **La APK de release ya no lleva los datos:** pesa unos 10 MB (antes 239). Cada release de Android en GitHub (tag `android-*`) lleva tres archivos: la APK, `zeq2data.zip` (los datos, el mismo zip que antes iba dentro) y `manifest.json` (versión, `versionCode`, SHA-256 de la APK, identificador de los datos, desde qué `versionCode` sirven esos datos y las notas del aviso). La APK de depuración sigue llevando los datos dentro y no busca actualizaciones.
- **Al abrir** (`SetupActivity`, `Updater.java`): pregunta a la API de GitHub por las releases y toma solo la más nueva con tag `android-*` (una más vieja devolvería datos viejos). Sin respuesta en 3 s, o si esa release no tiene `manifest.json` (las de antes del actualizador), el juego arranca. La lista se guarda con su ETag: una respuesta 304 no gasta del límite de 60 consultas por hora de la API sin cuenta.
- **APK nueva** (`versionCode` mayor): aviso con las notas y el tamaño; la baja a `cache/update`, comprueba el SHA-256 y se la pasa al instalador de Android por `ApkProvider` (un `content://` de solo lectura). Desde Android 8, la primera vez el jugador permite «Instalar apps desconocidas» para el juego (la app abre ese ajuste). Si los datos nuevos necesitan el código nuevo (`needsVersionCode`), primero va la APK.
- **Datos nuevos:** lee solo el final del zip remoto (el directorio central, unos 140 KB con 1678 archivos y su CRC32) y lo compara con el índice local `.zeq2data.index` (nombre, tamaño y CRC32 de cada archivo instalado). Baja solo los distintos, pidiendo esos tramos del zip con `Range` (GitHub responde 206); los vecinos a menos de 256 KB van en una sola petición. Cada archivo se escribe como `.zeq2part`, se comprueba su CRC y recién entonces reemplaza al viejo. Borra los que la versión nueva ya no trae. La primera vez es el mismo camino, con todos los archivos. Antes de bajar, avisa con la cantidad y el tamaño; la primera vez baja sin preguntar.
- **Cortes:** el índice se guarda cada 25 archivos y al fallar; «Reintentar» compara de nuevo y sigue desde el primer archivo que falta. Si la release se reemplaza en medio, el tamaño del zip en `Content-Range` ya no coincide y la descarga se detiene en vez de mezclar dos zips.
- **Datos de una versión anterior:** sin `.zeq2data.index`, la lista vieja (`.zeq2data.files`, solo nombres) se convierte leyendo el CRC32 de cada archivo una vez.
- `cl_updateCheck 0` (en `zeq2config.cfg`) apaga la búsqueda al abrir; sin datos instalados, igual los baja.
- **Consejos de carga** en `Android/data/touch/tips.txt` (uno por línea, `//` para comentarios): cambiarlos es una actualización de datos. Sin el archivo quedan los de `cg_info.c`.
- **Publicar:** `.\Build-Android.ps1 -Stage -Notes "..."` deja la APK, el zip y el manifest en `..\Build-Android\release\android-<versión>`; `-Publish -NotesFile notas.md` además crea la prerelease en el commit actual (tiene que estar subido; `-Replace` borra antes una release con el mismo tag). Un cambio solo de datos: `-Stage -DataOnly -Tag android-<versión>-d2`, que reusa la APK ya compilada de esa versión.
- **Probar sin GitHub:** `python Android/tools/update_server.py ..\Build-Android\release` imita la API y los archivos (con `Range`); `--only <tag>` muestra una sola release, `--cut-after <bytes>` corta cada descarga y `--kbps` la hace lenta. El móvil lo usa con `adb reverse tcp:8000 tcp:8000` y `--es updateFeed http://localhost:8000/releases`; el emulador con `http://10.0.2.2:8000/releases`. La app solo acepta esa dirección de prueba hacia `localhost`, `127.0.0.1` o `10.0.2.2`, y solo ahí permite HTTP sin cifrar (`res/xml/network_security_config.xml`).
- **Probado:** en el emulador de Android 15 (x86_64): instalación limpia (219 MB en 27 s desde el PC), sin novedades (arranca en medio segundo), APK nueva con el permiso y el instalador de Android, datos con 2 archivos cambiados (756 bytes), solo datos con un archivo borrado, corte cada 60 MB (siguió en tres reintentos) y sin conexión. Contra GitHub de verdad: ve la alpha 2 sin manifest y arranca. En el emulador de Android 7: instalación limpia y APK nueva por el instalador viejo, sin permiso por app.

## Estructura

| Ruta | Qué es |
|---|---|
| `CMakeLists.txt` | La parte nativa del APK. Sus listas de fuentes replican `Q3OBJ`, `CGOBJ_`, `GOBJ_` y `UIOBJ_` del `Makefile`: un archivo nuevo también hay que añadirlo aquí. |
| `Android/` | Proyecto Gradle: AGP 9.4.1 y wrapper de Gradle 9.8.1. La tarea `packZeqData` comprime `Build-Android/ZEQ2` y `Android/data` en `zeq2data.zip`: va dentro de la APK de depuración, y en las de release va a la release de GitHub (`-PzeqBundleData` lo cambia). |
| `.../SetupActivity.java` | Lanzador. Baja, actualiza o descomprime los datos y abre el juego, pasándole los argumentos `--es args "..."`. |
| `.../Updater.java`, `DataIndex.java`, `ApkProvider.java` | Actualizaciones desde las releases de GitHub: la release más nueva, el zip remoto por tramos, el índice de archivos instalados y la APK para el instalador. |
| `Android/data` | Datos propios de Android, versionados con el código: las imágenes táctiles (`tools/make_touch_art.py`) y `touch/tips.txt`. |
| `Android/tools/update_server.py` | Servidor de prueba que imita las releases de GitHub. |
| `.../ZEQ2Activity.java` | La actividad del juego, que hereda de `SDLActivity`. |
| `.../ZEQ2Surface.java` | La superficie del juego: dibuja a `r_androidHeight` y escala a la pantalla. |
| `Game/UI/ui_touch.c` | La página TOUCH CONTROLS de las opciones (diseño móvil). |
| `Android/third_party/SDL2` | SDL 2.32.10, copiado de `Source-LastRevision/deps/.sdl2`. |
| `Android/third_party/gl4es` | gl4es, commit `bc4314d` del 30-10-2026, sin los tests ni las trazas. Traduce OpenGL 1.x a GLES 2. |
| `Android/third_party/libogg`, `libvorbis` | libogg 1.3.3 y libvorbis 1.3.6, para la música. |
| `Build-Android.ps1` | Compila, instala y lanza el juego; prepara y publica las releases. |

## Requisitos (los de este PC)

- Android SDK en `D:\Android`, con platform 36, build-tools 36, NDK `29.0.14206865` y CMake `3.31.6`.
- JDK 27 en `C:\Program Files\Java\jdk-27`.
- Emulador: AVD `zeq2_api35` en `D:\Android\avd`. Antes de lanzarlo, define `ANDROID_AVD_HOME=D:\Android\avd`.
- Android 7 y 32 bits: AVD `zeq2_api24_x86` (Android 7.0, x86 de 32 bits). El emulador actual ya no arranca imágenes ARM de 32 bits, así que se prueba con una APK de x86: `gradlew.bat :app:assembleRelease -PzeqAbis=x86`. En x86 se compila con `C_ONLY`, porque el motor pediría el ensamblador de PC para el sonido y `Sys_SnapVector`.

## Uso

```powershell
.\Build-Android.ps1                              # APK arm64 y armeabi-v7a (32 bits), para un móvil
.\Build-Android.ps1 -Emulator                    # arm64 y x86_64, para el emulador
.\Build-Android.ps1 -Install -Abi armeabi-v7a    # instala el código de 32 bits en un móvil de 64, para probarlo
.\Build-Android.ps1 -Install -Run                # instalar, abrir y seguir el log
.\Build-Android.ps1 -NoBuild -Run -Map desert    # entrar directo en un mapa
.\Build-Android.ps1 -Clean -Install              # desinstalar antes: borra datos y configuración
.\Build-Android.ps1 -Release                     # APK de release firmada, sin datos: ..\Build-Android\ZEQ2-Lite-Mobile-<versión>.apk
.\Build-Android.ps1 -Stage -Notes "..."          # release preparada en ..\Build-Android\release\android-<versión>
.\Build-Android.ps1 -Publish -NotesFile notas.md -Notes "..."   # y además la crea en GitHub
.\Build-Android.ps1 -Run -Feed http://localhost:8000/releases   # contra el servidor de prueba
.\Build-Android.ps1 -Install -Device <serie>     # con varios dispositivos (adb devices)
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
- `Shared/q_platform.h`: `ARCH_STRING "arm64"` (en 32 bits ya existía `"arm"`).

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
- **Sonido (también en PC):** `players/vegeta*/default.grfx` pide `skills/energyBeam/beamCharging.ogg`, pero el archivo se llama `beamCharge.ogg`.
- **Ataques de ki (también en PC):** `g_usermissile.c` leía y escribía el cliente de entidades que no lo tienen (un ataque que choca con el mapa, que se borra sin haber tocado a nadie, o dos rayos que chocan). El QVM escribía en memoria baja de la VM sin cerrarse; en nativo cerraba el juego al pelear contra bots. Corregido en la rama `android`.
