@echo off
setlocal
echo Cleaning temporary build files...
taskkill /f /im Perdanga11.exe 2>nul
taskkill /f /im Perdanga11_Setup.exe 2>nul

if exist build rd /s /q build
echo [OK] All intermediate artifacts deleted.