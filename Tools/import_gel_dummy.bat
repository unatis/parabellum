@echo off
rem Imports Import/GelDummy/GelDummy.fbx into /Game/Range/GelDummy. No editor UI.
set UE="C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
%UE% "%~dp0..\Parabellum.uproject" -run=pythonscript -script="%~dp0import_gel_dummy.py" -unattended -stdout -FullStdOutLogOutput
