@echo off
setlocal EnableDelayedExpansion

echo =======================================================
echo                 Building Perdanga11
echo =======================================================

where cl.exe >nul 2>&1
if %ERRORLEVEL% equ 0 goto :compiler_found

echo [INFO] cl.exe not in PATH. Searching for MSVC environment...
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" set "VSWHERE=%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe"

if not exist "%VSWHERE%" goto :compiler_missing

for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
    set "VS_DIR=%%i"
)

if not defined VS_DIR goto :compiler_missing
if not exist "%VS_DIR%\VC\Auxiliary\Build\vcvars64.bat" goto :compiler_missing

echo [INFO] Initializing x64 Build Tools from: %VS_DIR%
call "%VS_DIR%\VC\Auxiliary\Build\vcvars64.bat" >nul

:compiler_found
where cl.exe >nul 2>&1
if %ERRORLEVEL% neq 0 goto :compiler_missing
goto :compile_start

:compiler_missing
echo [ERROR] MSVC C++ compiler (cl.exe) not found.
echo         Please run "install_build_tools.bat" to install the build environment,
echo         or open "x64 Native Tools Command Prompt for VS" and run build.bat again.
exit /b 1

:compile_start
taskkill /f /im Perdanga11.exe 2>nul
taskkill /f /im Perdanga11_Setup.exe 2>nul

if not exist bin mkdir bin
if not exist build mkdir build
if not exist dist mkdir dist

echo [1/4] Compiling Application Resources...
rc.exe /nologo /fo build\resource.res src\resource.rc
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Resource compilation failed.
    exit /b %ERRORLEVEL%
)

echo [2/4] Compiling Perdanga11.exe...
cl.exe /O2 /Os /EHsc /std:c++17 /utf-8 /I"src" /Fo"build\\" /Fe"bin\Perdanga11.exe" src\main.cpp build\resource.res /link /SUBSYSTEM:WINDOWS user32.lib gdi32.lib shell32.lib dwmapi.lib advapi32.lib ole32.lib oleaut32.lib comdlg32.lib comctl32.lib powrprof.lib gdiplus.lib uiautomationcore.lib
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Perdanga11.exe compilation failed.
    exit /b %ERRORLEVEL%
)

echo [3/4] Preparing Installer Artwork from Logo...
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$src = 'assets\logo\perdanga11.png';" ^
  "if (Test-Path $src) {" ^
  "  Add-Type -AssemblyName System.Drawing;" ^
  "  $img = [System.Drawing.Image]::FromFile((Resolve-Path $src));" ^
  "  $small = New-Object System.Drawing.Bitmap 55, 55;" ^
  "  $g1 = [System.Drawing.Graphics]::FromImage($small);" ^
  "  $g1.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic;" ^
  "  $g1.DrawImage($img, 0, 0, 55, 55);" ^
  "  $small.Save('assets\logo\logo_small.bmp', [System.Drawing.Imaging.ImageFormat]::Bmp);" ^
  "  $g1.Dispose(); $small.Dispose();" ^
  "  $banner = New-Object System.Drawing.Bitmap 164, 314;" ^
  "  $g2 = [System.Drawing.Graphics]::FromImage($banner);" ^
  "  $bgBrush = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(3, 12, 40));" ^
  "  $g2.FillRectangle($bgBrush, 0, 0, 164, 314);" ^
  "  $g2.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic;" ^
  "  $g2.DrawImage($img, 18, 93, 128, 128);" ^
  "  $banner.Save('assets\logo\wizard_banner.bmp', [System.Drawing.Imaging.ImageFormat]::Bmp);" ^
  "  $bgBrush.Dispose(); $g2.Dispose(); $banner.Dispose(); $img.Dispose();" ^
  "}"

echo [4/4] Compiling Inno Setup Installer...
set "ISCC_PATH=%LocalAppData%\Programs\Inno Setup 6\ISCC.exe"
if not exist "%ISCC_PATH%" set "ISCC_PATH=%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe"
if not exist "%ISCC_PATH%" set "ISCC_PATH=%ProgramFiles%\Inno Setup 6\ISCC.exe"
if not exist "%ISCC_PATH%" (
    where ISCC.exe >nul 2>&1
    if !ERRORLEVEL! equ 0 (
        for /f "tokens=*" %%a in ('where ISCC.exe') do set "ISCC_PATH=%%a"
    )
)

if exist "%ISCC_PATH%" (
    echo [INFO] Using Inno Setup compiler: "%ISCC_PATH%"
    "%ISCC_PATH%" "installer\setup.iss"
    if !ERRORLEVEL! equ 0 (
        echo [OK] Installer built successfully in dist\Perdanga11_Setup.exe
    ) else (
        echo [ERROR] Inno Setup compilation failed.
    )
) else (
    echo [INFO] Inno Setup compiler not found in standard paths.
    echo        Compile installer\setup.iss manually via Inno Setup GUI.
)

echo.
echo =======================================================
echo [OK] Build complete!
echo  - Main Executable: bin\Perdanga11.exe
echo  - Inno Installer:  dist\Perdanga11_Setup.exe
echo =======================================================

start "" "bin\Perdanga11.exe"
