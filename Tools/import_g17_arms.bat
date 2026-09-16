@echo off
rem Imports BarcodeGames G17 arms pack into /Game/Weapons/G17Arms. No editor UI.
set UE="C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
%UE% "%~dp0..\Parabellum.uproject" -run=pythonscript -script="%~dp0import_g17_arms.py" -unattended -stdout -FullStdOutLogOutput
