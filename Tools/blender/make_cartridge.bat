@echo off
rem Builds cartridge, case and bullet meshes from the SAAMI/CIP drawing. Output: Import\Ammo
"C:\Program Files\Blender Foundation\Blender 5.2\blender.exe" -b -P "%~dp0make_cartridge.py"
