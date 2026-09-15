@echo off
rem Imports Import/Mixamo character + Anims into /Game/Characters (no editor UI).
set UE="C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
%UE% "%~dp0..\Parabellum.uproject" -run=pythonscript -script="%~dp0import_mixamo.py" -unattended -stdout -FullStdOutLogOutput
