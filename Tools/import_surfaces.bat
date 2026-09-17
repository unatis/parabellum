@echo off
rem Imports bunker surface maps and builds the triplanar master material.
set UE="C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
%UE% "%~dp0..\Parabellum.uproject" -run=pythonscript -script="%~dp0import_surfaces.py" -unattended -stdout -FullStdOutLogOutput
