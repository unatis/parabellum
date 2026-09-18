@echo off
rem Builds Colt M1911 (1911 model) parts from published dimensions. Output: Import\Colt1911Parts
"C:\Program Files\Blender Foundation\Blender 5.2\blender.exe" -b -P "%~dp0make_colt1911.py"
