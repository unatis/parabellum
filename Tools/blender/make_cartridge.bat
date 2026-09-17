@echo off
rem Builds the 9x19 cartridge, fired case and bullet. Output: Import\Ammo
"C:\Program Files\Blender Foundation\Blender 5.2lender.exe" -b -P "%~dp0make_cartridge.py"
