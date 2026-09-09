@echo off
rem Opens the project in Unreal Editor.
set UE="C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
set PROJECT="%~dp0Parabellum.uproject"
start "" %UE% %PROJECT% %*
