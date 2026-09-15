@echo off
rem Copies Lyra pistol assets (+deps) into this project. Runs the python inside the Lyra project.
set UE="C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
set LYRA=C:\UnrealLiraProj\LyraStarterGame
set PBL_LYRA_CONTENT=%LYRA%\Content
set PBL_DST_CONTENT=%~dp0..\Content
%UE% "%LYRA%\Lyra.uproject" -run=pythonscript -script="%~dp0migrate_from_lyra.py" -unattended -stdout -FullStdOutLogOutput
