@echo off
rem Builds the hand skin texture from the user photos. Output: Import\Hands\Hand_D.png
"C:\Program Files\Blender Foundation\Blender 5.2lender.exe" -b -P "%~dp0make_hand_texture.py"
