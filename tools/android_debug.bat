@echo off
rem VBoy Color - install a build on an Android phone (or Quest) over USB and
rem watch its log (a crash, a game that won't load, ...).
rem
rem   1. Put this file next to the APK (VBoyColor-...-quest-android-arm64.apk -
rem      the newest one in the folder is used), or drag an APK onto it.
rem   2. Turn on USB debugging on the phone (Settings > About phone > tap
rem      "Build number" 7 times, then Developer options > USB debugging),
rem      connect it and allow the computer when it asks.
rem   3. Run this. It installs (keeping your games and settings), starts the
rem      app and shows its log - also saved as vboycolor-log.txt here.

setlocal enabledelayedexpansion
cd /d "%~dp0"
set PACKAGE=io.github.yuk27.vboycolor

rem --- adb: on the PATH, or Android Studio's SDK ---
set ADB=adb
where adb >nul 2>nul
if errorlevel 1 (
    set ADB=%LOCALAPPDATA%\Android\Sdk\platform-tools\adb.exe
    if not exist "!ADB!" (
        echo adb wasn't found. Install Android Studio, or get "SDK Platform Tools":
        echo   https://developer.android.com/tools/releases/platform-tools
        echo and put adb.exe next to this file or on the PATH.
        pause
        exit /b 1
    )
)

rem --- the APK: dragged onto this file, or the newest one here ---
set APK=%~1
if "%APK%"=="" (
    for /f "delims=" %%f in ('dir /b /o-d "VBoyColor-*-quest-android-arm64.apk" 2^>nul') do (
        if "!APK!"=="" set APK=%%f
    )
)
if "%APK%"=="" (
    echo No VBoyColor-...-quest-android-arm64.apk next to this file.
    pause
    exit /b 1
)

echo Waiting for the phone (connected over USB, USB debugging allowed)...
"%ADB%" wait-for-device
echo.
echo Installing %APK% ...
"%ADB%" install -r "%APK%"
if errorlevel 1 (
    echo.
    echo The install failed. If it says the signatures don't match, uninstall
    echo VBoy Color from the phone first - that deletes its settings, but not
    echo the games in your ROMs folder.
    pause
    exit /b 1
)

echo.
echo Starting VBoy Color...
"%ADB%" logcat -c
"%ADB%" shell am start -n %PACKAGE%/.MainActivity >nul

echo.
echo VBoy Color's log (also in vboycolor-log.txt) - Ctrl+C to stop.
echo.
powershell -NoProfile -Command "& '%ADB%' logcat -v time -s VBoyColor:V | Tee-Object -FilePath 'vboycolor-log.txt'"
pause
