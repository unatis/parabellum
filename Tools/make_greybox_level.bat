@echo off
rem Regenerates /Game/Maps/Greybox from Tools/make_greybox_level.py (no editor UI).
set UE="C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
%UE% "%~dp0..\Parabellum.uproject" -run=pythonscript -script="%~dp0make_greybox_level.py" -unattended -stdout -FullStdOutLogOutput
