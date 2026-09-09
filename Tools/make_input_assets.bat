@echo off
rem Regenerates Enhanced Input assets (Content/Input) from Tools/make_input_assets.py.
set UE="C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
%UE% "%~dp0..\Parabellum.uproject" -run=pythonscript -script="%~dp0make_input_assets.py" -unattended -stdout -FullStdOutLogOutput
