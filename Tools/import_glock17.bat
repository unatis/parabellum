@echo off
rem Imports Import/Glock17/unpacked into /Game/Weapons/Glock17. No editor UI.
set UE="C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
%UE% "%~dp0..\Parabellum.uproject" -run=pythonscript -script="%~dp0import_glock17.py" -unattended -stdout -FullStdOutLogOutput
