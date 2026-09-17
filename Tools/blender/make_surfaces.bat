@echo off
rem Generates tiling bunker surface maps. Output: Import\Surfaces
"C:\Program Files\Blender Foundation\Blender 5.2lender.exe" -b -P "%~dp0make_surfaces.py"
