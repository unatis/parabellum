@echo off
rem Launches Parabellum as a game (no editor UI). Double-click or run from cmd.
rem Optional args are passed through, e.g.:  Play.bat -ResX=1920 -ResY=1080
set UE="C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
set PROJECT="%~dp0Parabellum.uproject"
start "" %UE% %PROJECT% -game -windowed -ResX=1280 -ResY=720 %*
