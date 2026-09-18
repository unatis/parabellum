@echo off
rem Builds hand skin albedo/normal/roughness from the photos in Import\Hands. Output: Import\Hands
"C:\Program Files\Blender Foundation\Blender 5.2\blender.exe" -b -P "%~dp0make_hand_texture.py"
