@echo off
rem Builds Glock 17 field-strip parts from published dimensions. Output: Import\Glock17Parts
"C:\Program Files\Blender Foundation\Blender 5.2\blender.exe" -b -P "%~dp0make_glock17.py"
