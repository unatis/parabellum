@echo off
rem Imports the 9x19 cartridge, fired case and bullet meshes. No editor UI.
set UE="C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
%UE% "%~dp0..\Parabellum.uproject" -run=pythonscript -script="%~dp0import_ammo.py" -unattended -stdout -FullStdOutLogOutput
