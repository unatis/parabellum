@echo off
rem Builds range assets (gel and wound-channel materials). No editor UI.
set UE="C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
%UE% "%~dp0..\Parabellum.uproject" -run=pythonscript -script="%~dp0make_range_assets.py" -unattended -stdout -FullStdOutLogOutput
