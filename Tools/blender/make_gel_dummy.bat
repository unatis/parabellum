@echo off
rem Builds the gel mannequin FBX with Blender (headless). Output: Import\GelDummy\GelDummy.fbx + .json
"C:\Program Files\Blender Foundation\Blender 5.2\blender.exe" -b -P "%~dp0make_gel_dummy.py"
