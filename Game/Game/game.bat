@echo off
md vm
cd vm

set cc=call ..\..\..\Tools\Compiler\Perform.bat
if "%1"=="TA" goto TA

%cc%  ../g_main.c
@if errorlevel 1 goto quit
%cc%  ../bg_misc.c
@if errorlevel 1 goto quit
%cc%  ../bg_lib.c
@if errorlevel 1 goto quit
%cc%  ../bg_pmove.c
@if errorlevel 1 goto quit
%cc%  ../bg_slidemove.c
@if errorlevel 1 goto quit
%cc%  ../../../Shared/q_math.c
@if errorlevel 1 goto quit
%cc%  ../../../Shared/q_shared.c
@if errorlevel 1 goto quit
%cc%  ../g_active.c
@if errorlevel 1 goto quit
%cc%  ../g_arenas.c
@if errorlevel 1 goto quit
%cc%  ../g_client.c
@if errorlevel 1 goto quit
%cc%  ../g_cmds.c
@if errorlevel 1 goto quit
%cc%  ../g_combat.c
@if errorlevel 1 goto quit
%cc%  ../g_mem.c
@if errorlevel 1 goto quit
%cc%  ../g_misc.c
@if errorlevel 1 goto quit
%cc%  ../g_missile.c
@if errorlevel 1 goto quit
%cc%  ../g_mover.c
@if errorlevel 1 goto quit
%cc%  ../g_session.c
@if errorlevel 1 goto quit
%cc%  ../g_spawn.c
@if errorlevel 1 goto quit
%cc%  ../g_svcmds.c
@if errorlevel 1 goto quit
%cc%  ../g_target.c
@if errorlevel 1 goto quit
%cc%  ../g_team.c
@if errorlevel 1 goto quit
%cc%  ../g_trigger.c
@if errorlevel 1 goto quit
%cc%  ../g_utils.c
@if errorlevel 1 goto quit
%cc%  ../g_weapon.c
@if errorlevel 1 goto quit
%cc%  ../g_usermissile.c
@if errorlevel 1 goto quit
%cc%  ../g_userweapons.c
@if errorlevel 1 goto quit
%cc%  ../g_tiers.c
@if errorlevel 1 goto quit
%cc%  ../g_radar.c
@if errorlevel 1 goto quit
%cc%  ../g_botlite_main.c
@if errorlevel 1 goto quit
%cc%  ../g_botlite_action.c
@if errorlevel 1 goto quit
%cc%  ../g_botlite_profile.c
@if errorlevel 1 goto quit
%cc%  ../g_botlite_skill.c
@if errorlevel 1 goto quit
%cc%  ../g_botlite_ai.c
@if errorlevel 1 goto quit
%cc%  ../g_botlite_target.c
@if errorlevel 1 goto quit
%cc%  ../g_botlite_lockon.c
@if errorlevel 1 goto quit
%cc%  ../g_botlite_movement.c
@if errorlevel 1 goto quit
%cc%  ../g_botlite_tactics.c
@if errorlevel 1 goto quit
%cc%  ../g_botlite_melee.c
@if errorlevel 1 goto quit
%cc%  ../g_botlite_ranged.c
@if errorlevel 1 goto quit
%cc%  ../g_botlite_skill1.c
@if errorlevel 1 goto quit
%cc%  ../g_botlite_skill2.c
%cc%  ../g_botlite_skill3.c
@if errorlevel 1 goto quit
%cc%  ../g_botlite_struggle.c
@if errorlevel 1 goto quit
%cc%  ../g_botlite_resource.c
@if errorlevel 1 goto quit
%cc%  ../g_botlite_transform.c
@if errorlevel 1 goto quit
%cc%  ../g_botlite_dodge.c
@if errorlevel 1 goto quit
%cc%  ../g_botlite_engage.c
@if errorlevel 1 goto quit
%cc%  ../g_botlite_zanzoken.c
@if errorlevel 1 goto quit
%cc%  ../g_weapPhysParser.c
@if errorlevel 1 goto quit
%cc%  ../g_weapPhysScanner.c
@if errorlevel 1 goto quit
%cc%  ../g_weapPhysAttributes.c
@if errorlevel 1 goto quit


..\..\..\Tools\Compiler\q3asm -f ../game
:quit
del *.asm
cd ..
rd vm