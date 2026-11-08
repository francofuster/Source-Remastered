# ZEQ2 Remaster for Android: native part of the APK.
#
# Gradle (Android/app/build.gradle) runs this file once per ABI. It builds
#   libSDL2.so   window, input and audio, with the SDLActivity Java side
#   libmain.so   the engine, with gl4es linked in as its OpenGL
#   libcgame.so, libgame.so, libui.so   the game modules, native on Android
#
# The source lists mirror Q3OBJ, CGOBJ_, GOBJ_ and UIOBJ_ in the Makefile.
# A new source file also has to be added here.

cmake_minimum_required(VERSION 3.22)
project(ZEQ2Android C CXX)

set(TP ${CMAKE_CURRENT_SOURCE_DIR}/Android/third_party)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_EXTENSIONS ON)

# char is unsigned on ARM and the game code was written for x86.
add_compile_options(-fsigned-char)

# --- SDL 2 ----------------------------------------------------------------
set(SDL_SHARED ON CACHE BOOL "" FORCE)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
set(SDL2_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
add_subdirectory(${TP}/SDL2 SDL2 EXCLUDE_FROM_ALL)

set(SRC_ENGINE_CLIENT
  Engine/client/cl_cgame.c
  Engine/client/cl_cin.c
  Engine/client/cl_console.c
  Engine/client/cl_input.c
  Engine/client/cl_keys.c
  Engine/client/cl_main.c
  Engine/client/cl_net_chan.c
  Engine/client/cl_parse.c
  Engine/client/cl_scrn.c
  Engine/client/cl_ui.c
  Engine/client/cl_avi.c
  Engine/client/snd_adpcm.c
  Engine/client/snd_dma.c
  Engine/client/snd_mem.c
  Engine/client/snd_mix.c
  Engine/client/snd_wavelet.c
  Engine/client/snd_main.c
  Engine/client/snd_codec.c
  Engine/client/snd_codec_wav.c
  Engine/client/snd_codec_ogg.c
)

set(SRC_SHARED
  Shared/cm_load.c
  Shared/cm_patch.c
  Shared/cm_polylib.c
  Shared/cm_test.c
  Shared/cm_trace.c
  Shared/cmd.c
  Shared/common.c
  Shared/cvar.c
  Shared/files.c
  Shared/md4.c
  Shared/md5.c
  Shared/msg.c
  Shared/net_chan.c
  Shared/net_ip.c
  Shared/huffman.c
  Shared/q_math.c
  Shared/q_shared.c
  Shared/unzip.c
  Shared/ioapi.c
  Shared/puff.c
  Shared/vm.c
  Shared/vm_interpreted.c
)

set(SRC_ENGINE_SERVER
  Engine/server/sv_bot.c
  Engine/server/sv_ccmds.c
  Engine/server/sv_client.c
  Engine/server/sv_game.c
  Engine/server/sv_init.c
  Engine/server/sv_main.c
  Engine/server/sv_net_chan.c
  Engine/server/sv_snapshot.c
  Engine/server/sv_world.c
)

set(SRC_ENGINE_BOTLIB
  Engine/botlib/be_aas_bspq3.c
  Engine/botlib/be_aas_cluster.c
  Engine/botlib/be_aas_debug.c
  Engine/botlib/be_aas_entity.c
  Engine/botlib/be_aas_file.c
  Engine/botlib/be_aas_main.c
  Engine/botlib/be_aas_move.c
  Engine/botlib/be_aas_optimize.c
  Engine/botlib/be_aas_reach.c
  Engine/botlib/be_aas_route.c
  Engine/botlib/be_aas_routealt.c
  Engine/botlib/be_aas_sample.c
  Engine/botlib/be_ai_char.c
  Engine/botlib/be_ai_chat.c
  Engine/botlib/be_ai_gen.c
  Engine/botlib/be_ai_goal.c
  Engine/botlib/be_ai_move.c
  Engine/botlib/be_ai_weap.c
  Engine/botlib/be_ai_weight.c
  Engine/botlib/be_ea.c
  Engine/botlib/be_interface.c
  Engine/botlib/l_crc.c
  Engine/botlib/l_libvar.c
  Engine/botlib/l_log.c
  Engine/botlib/l_memory.c
  Engine/botlib/l_precomp.c
  Engine/botlib/l_script.c
  Engine/botlib/l_struct.c
)

set(SRC_ENGINE_JPEG_6B
  Engine/jpeg-6b/jcapimin.c
  Engine/jpeg-6b/jcapistd.c
  Engine/jpeg-6b/jccoefct.c
  Engine/jpeg-6b/jccolor.c
  Engine/jpeg-6b/jcdctmgr.c
  Engine/jpeg-6b/jchuff.c
  Engine/jpeg-6b/jcinit.c
  Engine/jpeg-6b/jcmainct.c
  Engine/jpeg-6b/jcmarker.c
  Engine/jpeg-6b/jcmaster.c
  Engine/jpeg-6b/jcomapi.c
  Engine/jpeg-6b/jcparam.c
  Engine/jpeg-6b/jcphuff.c
  Engine/jpeg-6b/jcprepct.c
  Engine/jpeg-6b/jcsample.c
  Engine/jpeg-6b/jdapimin.c
  Engine/jpeg-6b/jdapistd.c
  Engine/jpeg-6b/jdatasrc.c
  Engine/jpeg-6b/jdcoefct.c
  Engine/jpeg-6b/jdcolor.c
  Engine/jpeg-6b/jddctmgr.c
  Engine/jpeg-6b/jdhuff.c
  Engine/jpeg-6b/jdinput.c
  Engine/jpeg-6b/jdmainct.c
  Engine/jpeg-6b/jdmarker.c
  Engine/jpeg-6b/jdmaster.c
  Engine/jpeg-6b/jdpostct.c
  Engine/jpeg-6b/jdsample.c
  Engine/jpeg-6b/jdtrans.c
  Engine/jpeg-6b/jerror.c
  Engine/jpeg-6b/jfdctflt.c
  Engine/jpeg-6b/jidctflt.c
  Engine/jpeg-6b/jmemmgr.c
  Engine/jpeg-6b/jmemnobs.c
  Engine/jpeg-6b/jutils.c
)

set(SRC_ENGINE_RENDERER
  Engine/renderer/tr_animation.c
  Engine/renderer/tr_backend.c
  Engine/renderer/tr_bloom.c
  Engine/renderer/tr_bsp.c
  Engine/renderer/tr_cmds.c
  Engine/renderer/tr_curve.c
  Engine/renderer/tr_flares.c
  Engine/renderer/tr_font.c
  Engine/renderer/tr_glsl.c
  Engine/renderer/tr_image.c
  Engine/renderer/tr_image_png.c
  Engine/renderer/tr_image_jpg.c
  Engine/renderer/tr_image_bmp.c
  Engine/renderer/tr_image_tga.c
  Engine/renderer/tr_image_pcx.c
  Engine/renderer/tr_init.c
  Engine/renderer/tr_light.c
  Engine/renderer/tr_main.c
  Engine/renderer/tr_marks.c
  Engine/renderer/tr_md4.c
  Engine/renderer/tr_mesh.c
  Engine/renderer/tr_model.c
  Engine/renderer/tr_motionblur.c
  Engine/renderer/tr_noise.c
  Engine/renderer/tr_scene.c
  Engine/renderer/tr_shade.c
  Engine/renderer/tr_shade_calc.c
  Engine/renderer/tr_shader.c
  Engine/renderer/tr_shadows.c
  Engine/renderer/tr_sky.c
  Engine/renderer/tr_surface.c
  Engine/renderer/tr_world.c
)

set(SRC_ENGINE_SDL
  Engine/sdl/sdl_gamma.c
  Engine/sdl/sdl_input.c
  Engine/sdl/sdl_snd.c
)

set(SRC_ENGINE_SYS
  Engine/sys/con_passive.c
  Engine/sys/con_log.c
  Engine/sys/sys_main.c
)

set(SRC_ENGINE_ZLIB
)

set(SRC_CGAME
  Game/CGame/cg_main.c
  Game/Game/bg_misc.c
  Game/Game/bg_pmove.c
  Game/Game/bg_slidemove.c
  Game/Game/bg_lib.c
  Game/CGame/cg_consolecmds.c
  Game/CGame/cg_draw.c
  Game/CGame/cg_drawtools.c
  Game/CGame/cg_effects.c
  Game/CGame/cg_ents.c
  Game/CGame/cg_event.c
  Game/CGame/cg_info.c
  Game/CGame/cg_localents.c
  Game/CGame/cg_marks.c
  Game/CGame/cg_players.c
  Game/CGame/cg_playerstate.c
  Game/CGame/cg_predict.c
  Game/CGame/cg_servercmds.c
  Game/CGame/cg_snapshot.c
  Game/CGame/cg_view.c
  Game/CGame/cg_lockcam.c
  Game/CGame/cg_weapons.c
  Game/CGame/cg_auras.c
  Game/CGame/cg_userweapons.c
  Game/CGame/cg_beamtables.c
  Game/CGame/cg_particlesystem.c
  Game/CGame/cg_particlesystem_cache.c
  Game/CGame/cg_trails.c
  Game/CGame/cg_radar.c
  Game/CGame/cg_weapGfxParser.c
  Game/CGame/cg_weapGfxScanner.c
  Game/CGame/cg_weapGfxAttributes.c
  Game/CGame/cg_tiers.c
  Game/CGame/cg_music.c
  Game/CGame/cg_frameHist.c
  Game/CGame/cg_motionblur.c
  Shared/q_math.c
  Shared/q_shared.c
  Game/CGame/cg_syscalls.c
)

set(SRC_GAME
  Game/Game/g_main.c
  Game/Game/bg_misc.c
  Game/Game/bg_pmove.c
  Game/Game/bg_slidemove.c
  Game/Game/bg_lib.c
  Game/Game/g_active.c
  Game/Game/g_arenas.c
  Game/Game/g_client.c
  Game/Game/g_cmds.c
  Game/Game/g_combat.c
  Game/Game/g_mem.c
  Game/Game/g_misc.c
  Game/Game/g_missile.c
  Game/Game/g_mover.c
  Game/Game/g_session.c
  Game/Game/g_spawn.c
  Game/Game/g_svcmds.c
  Game/Game/g_target.c
  Game/Game/g_team.c
  Game/Game/g_trigger.c
  Game/Game/g_utils.c
  Game/Game/g_weapon.c
  Game/Game/g_usermissile.c
  Game/Game/g_userweapons.c
  Game/Game/g_tiers.c
  Game/Game/g_radar.c
  Game/Game/g_botlite_main.c
  Game/Game/g_botlite_action.c
  Game/Game/g_botlite_profile.c
  Game/Game/g_botlite_skill.c
  Game/Game/g_botlite_ai.c
  Game/Game/g_botlite_target.c
  Game/Game/g_botlite_lockon.c
  Game/Game/g_botlite_movement.c
  Game/Game/g_botlite_tactics.c
  Game/Game/g_botlite_melee.c
  Game/Game/g_botlite_ranged.c
  Game/Game/g_botlite_skill1.c
  Game/Game/g_botlite_skill2.c
  Game/Game/g_botlite_skill3.c
  Game/Game/g_botlite_struggle.c
  Game/Game/g_botlite_resource.c
  Game/Game/g_botlite_transform.c
  Game/Game/g_botlite_dodge.c
  Game/Game/g_botlite_engage.c
  Game/Game/g_botlite_zanzoken.c
  Game/Game/g_weapPhysParser.c
  Game/Game/g_weapPhysScanner.c
  Game/Game/g_weapPhysAttributes.c
  Shared/q_math.c
  Shared/q_shared.c
  Game/Game/g_syscalls.c
)

set(SRC_UI
  Game/UI/ui_main.c
  Game/Game/bg_misc.c
  Game/Game/bg_lib.c
  Game/UI/ui_atoms.c
  Game/UI/ui_confirm.c
  Game/UI/ui_connect.c
  Game/UI/ui_controls2.c
  Game/UI/ui_credits.c
  Game/UI/ui_gameinfo.c
  Game/UI/ui_ingame.c
  Game/UI/ui_menu.c
  Game/UI/ui_mfield.c
  Game/UI/ui_options.c
  Game/UI/ui_playermodel.c
  Game/UI/ui_players.c
  Game/UI/ui_preferences.c
  Game/UI/ui_qmenu.c
  Game/UI/ui_serverinfo.c
  Game/UI/ui_servers2.c
  Game/UI/ui_specifyserver.c
  Game/UI/ui_shared.c
  Game/UI/ui_bots.c
  Game/UI/ui_startserver.c
  Game/UI/ui_system.c
  Game/UI/ui_camera.c
  Shared/q_math.c
  Shared/q_shared.c
  Game/UI/ui_syscalls.c
)

set(SRC_GL4ES
  ${TP}/gl4es/src/gl/arbconverter.c
  ${TP}/gl4es/src/gl/arbgenerator.c
  ${TP}/gl4es/src/gl/arbhelper.c
  ${TP}/gl4es/src/gl/arbparser.c
  ${TP}/gl4es/src/gl/array.c
  ${TP}/gl4es/src/gl/blend.c
  ${TP}/gl4es/src/gl/blit.c
  ${TP}/gl4es/src/gl/buffers.c
  ${TP}/gl4es/src/gl/build_info.c
  ${TP}/gl4es/src/gl/debug.c
  ${TP}/gl4es/src/gl/decompress.c
  ${TP}/gl4es/src/gl/depth.c
  ${TP}/gl4es/src/gl/directstate.c
  ${TP}/gl4es/src/gl/drawing.c
  ${TP}/gl4es/src/gl/enable.c
  ${TP}/gl4es/src/gl/envvars.c
  ${TP}/gl4es/src/gl/eval.c
  ${TP}/gl4es/src/gl/face.c
  ${TP}/gl4es/src/gl/fog.c
  ${TP}/gl4es/src/gl/fpe.c
  ${TP}/gl4es/src/gl/fpe_cache.c
  ${TP}/gl4es/src/gl/fpe_shader.c
  ${TP}/gl4es/src/gl/framebuffers.c
  ${TP}/gl4es/src/gl/gl_lookup.c
  ${TP}/gl4es/src/gl/getter.c
  ${TP}/gl4es/src/gl/gl4es.c
  ${TP}/gl4es/src/gl/glstate.c
  ${TP}/gl4es/src/gl/hint.c
  ${TP}/gl4es/src/gl/init.c
  ${TP}/gl4es/src/gl/light.c
  ${TP}/gl4es/src/gl/line.c
  ${TP}/gl4es/src/gl/list.c
  ${TP}/gl4es/src/gl/listdraw.c
  ${TP}/gl4es/src/gl/listrl.c
  ${TP}/gl4es/src/gl/loader.c
  ${TP}/gl4es/src/gl/logs.c
  ${TP}/gl4es/src/gl/matrix.c
  ${TP}/gl4es/src/gl/matvec.c
  ${TP}/gl4es/src/gl/oldprogram.c
  ${TP}/gl4es/src/gl/pixel.c
  ${TP}/gl4es/src/gl/planes.c
  ${TP}/gl4es/src/gl/pointsprite.c
  ${TP}/gl4es/src/gl/preproc.c
  ${TP}/gl4es/src/gl/program.c
  ${TP}/gl4es/src/gl/queries.c
  ${TP}/gl4es/src/gl/raster.c
  ${TP}/gl4es/src/gl/render.c
  ${TP}/gl4es/src/gl/samplers.c
  ${TP}/gl4es/src/gl/shader.c
  ${TP}/gl4es/src/gl/shaderconv.c
  ${TP}/gl4es/src/gl/shader_hacks.c
  ${TP}/gl4es/src/gl/stack.c
  ${TP}/gl4es/src/gl/stencil.c
  ${TP}/gl4es/src/gl/string_utils.c
  ${TP}/gl4es/src/gl/stubs.c
  ${TP}/gl4es/src/gl/texenv.c
  ${TP}/gl4es/src/gl/texgen.c
  ${TP}/gl4es/src/gl/texture.c
  ${TP}/gl4es/src/gl/texture_compressed.c
  ${TP}/gl4es/src/gl/texture_params.c
  ${TP}/gl4es/src/gl/texture_read.c
  ${TP}/gl4es/src/gl/texture_3d.c
  ${TP}/gl4es/src/gl/uniform.c
  ${TP}/gl4es/src/gl/vertexattrib.c
  ${TP}/gl4es/src/gl/wrap/gl4eswraps.c
  ${TP}/gl4es/src/gl/wrap/gles.c
  ${TP}/gl4es/src/gl/wrap/glstub.c
  ${TP}/gl4es/src/gl/math/matheval.c
  ${TP}/gl4es/src/glx/hardext.c
  ${TP}/gl4es/src/glx/glx.c
  ${TP}/gl4es/src/glx/lookup.c
  ${TP}/gl4es/src/glx/gbm.c
  ${TP}/gl4es/src/glx/streaming.c
)

set(SRC_ENGINE_EXTRA
  Engine/client/cl_aimassist.c
  Engine/client/cl_touch.c
  Engine/client/snd_openal.c
  Engine/sdl/sdl_glimp.c
  Engine/sys/sys_unix.c
  Engine/zlib/adler32.c
  Engine/zlib/crc32.c
  Engine/zlib/inffast.c
  Engine/zlib/inflate.c
  Engine/zlib/inftrees.c
  Engine/zlib/zutil.c
)

# --- gl4es: OpenGL 1.x on top of GLES 2 ------------------------------------
add_library(gl4es STATIC ${SRC_GL4ES})
target_include_directories(gl4es PRIVATE ${TP}/gl4es/include)
target_compile_definitions(gl4es PRIVATE
  ANDROID NOX11 NOEGL NO_GBM NO_INIT_CONSTRUCTOR DEFAULT_ES=2 STATICLIB)
target_compile_options(gl4es PRIVATE
  -std=gnu99 -O3 -fvisibility=hidden -funwind-tables -w
  -include ${TP}/gl4es/include/android_debug.h)

# --- ogg and vorbis, for the music ------------------------------------------
add_library(ogg STATIC ${TP}/libogg/bitwise.c ${TP}/libogg/framing.c)
target_include_directories(ogg PUBLIC ${TP}/libogg/include)
target_compile_options(ogg PRIVATE -O2 -w)

set(VORBIS_SRC analysis bitrate block codebook envelope floor0 floor1 info
  lookup lpc lsp mapping0 mdct psy registry res0 sharedbook smallft synthesis
  vorbisfile window)
list(TRANSFORM VORBIS_SRC PREPEND ${TP}/libvorbis/lib/)
list(TRANSFORM VORBIS_SRC APPEND .c)
add_library(vorbis STATIC ${VORBIS_SRC})
target_include_directories(vorbis PUBLIC ${TP}/libvorbis/include
  PRIVATE ${TP}/libvorbis/lib)
target_compile_options(vorbis PRIVATE -O2 -w)
target_link_libraries(vorbis PUBLIC ogg)

# --- Shared flags for the engine and the game modules ------------------------
set(ZEQ_DEFS
  NDEBUG
  NO_VM_COMPILED
  NO_GZIP
  PRODUCT_VERSION="PB2")

# A cast that loses half of a pointer is a real bug on 64 bits, and a variable
# read before it is set held zero in the QVM but holds garbage natively (it
# froze the game: PM_Friction). Both fail the build.
set(ZEQ_OPTS
  -O2 -fno-strict-aliasing
  -Wno-unused-but-set-variable -Wno-unused-variable -Wno-unused-function
  -Wno-parentheses -Wno-dangling-else -Wno-misleading-indentation
  -Wno-pointer-sign -Wno-comment -Wno-unused-value
  -Werror=pointer-to-int-cast -Werror=int-to-pointer-cast
  -Werror=implicit-function-declaration -Werror=return-type
  -Werror=uninitialized -Werror=sometimes-uninitialized)

# --- Engine: libmain.so -----------------------------------------------------
add_library(main SHARED
  ${SRC_ENGINE_CLIENT} ${SRC_SHARED} ${SRC_ENGINE_SERVER} ${SRC_ENGINE_BOTLIB}
  ${SRC_ENGINE_JPEG_6B} ${SRC_ENGINE_RENDERER} ${SRC_ENGINE_SDL}
  ${SRC_ENGINE_SYS} ${SRC_ENGINE_EXTRA})
set_source_files_properties(${SRC_ENGINE_BOTLIB}
  PROPERTIES COMPILE_DEFINITIONS BOTLIB)
target_include_directories(main PRIVATE
  ${TP}/SDL2/include ${TP}/gl4es/include Engine/zlib)
target_compile_definitions(main PRIVATE ${ZEQ_DEFS} USE_CODEC_VORBIS USE_GL4ES USE_TOUCH)
target_compile_options(main PRIVATE ${ZEQ_OPTS} -ffast-math)
target_link_libraries(main PRIVATE SDL2::SDL2 gl4es vorbis ogg log android m)

# --- Game modules: libcgame.so, libgame.so, libui.so -------------------------
function(zeq_module name define)
  add_library(${name} SHARED ${ARGN})
  target_compile_definitions(${name} PRIVATE ${ZEQ_DEFS} ${define})
  target_compile_options(${name} PRIVATE ${ZEQ_OPTS} -fvisibility=hidden)
  target_link_libraries(${name} PRIVATE m)
endfunction()

zeq_module(cgame CGAME ${SRC_CGAME})
zeq_module(game GAME ${SRC_GAME})
zeq_module(ui UI ${SRC_UI})
