@echo off
REM Wrapper para Build.ps1 -- compila los QVM y verifica que lleguen al build.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Build.ps1" %*
