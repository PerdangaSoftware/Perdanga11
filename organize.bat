@echo off
setlocal
echo Organizing Perdanga11 project files...

taskkill /f /im Perdanga11.exe 2>nul
taskkill /f /im Perdanga11_Setup.exe 2>nul

mkdir assets 2>nul
mkdir src 2>nul
mkdir installer 2>nul
mkdir bin 2>nul
mkdir build 2>nul

:: Move assets
if exist ico move /y ico assets\ >nul
if exist logo move /y logo assets\ >nul

:: Move application source files
if exist AppIndexer.hpp move /y AppIndexer.hpp src\ >nul
if exist Config.hpp move /y Config.hpp src\ >nul
if exist Hooks.hpp move /y Hooks.hpp src\ >nul
if exist MenuInteraction.hpp move /y MenuInteraction.hpp src\ >nul
if exist MenuRenderer.hpp move /y MenuRenderer.hpp src\ >nul
if exist MenuState.hpp move /y MenuState.hpp src\ >nul
if exist MenuWindow.hpp move /y MenuWindow.hpp src\ >nul
if exist main.cpp move /y main.cpp src\ >nul
if exist resource.h move /y resource.h src\ >nul
if exist resource.rc move /y resource.rc src\ >nul

:: Move installer files
if exist installer.cpp move /y installer.cpp installer\ >nul
if exist installer.rc move /y installer.rc installer\ >nul
if exist setup.iss move /y setup.iss installer\ >nul

:: Move outputs & config to bin
if exist config.ini move /y config.ini bin\ >nul
if exist Perdanga11.exe move /y Perdanga11.exe bin\ >nul
if exist Perdanga11_Setup.exe move /y Perdanga11_Setup.exe bin\ >nul

:: Delete garbage and temporary build artifacts
if exist Perdanga11.cpp del /f /q Perdanga11.cpp
if exist *.obj del /f /q *.obj
if exist *.res del /f /q *.res

echo.
echo [OK] All files organized successfully!
pause