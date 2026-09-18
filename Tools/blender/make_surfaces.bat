@echo off
rem Builds the procedural surface maps (concrete, steel, plywood...). Output: Import\Surfaces
"C:\Program Files\Blender Foundation\Blender 5.2\blender.exe" -b -P "%~dp0make_surfaces.py"
