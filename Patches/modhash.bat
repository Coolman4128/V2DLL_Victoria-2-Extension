@echo off
rem Fingerprint of this game folder (see modhash.ps1). Put next to v2game.exe and run.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0modhash.ps1" -Root "%~dp0."
pause
