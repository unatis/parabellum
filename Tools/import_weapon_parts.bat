@echo off
rem Imports weapon parts and writes Content/Data/WeaponParts.csv. No editor UI.
set UE="C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
%UE% "%~dp0..\Parabellum.uproject" -run=pythonscript -script="%~dp0import_weapon_parts.py" -unattended -stdout -FullStdOutLogOutput
