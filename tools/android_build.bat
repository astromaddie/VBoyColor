@echo off
rem VBoy Color - build the Android app on this PC and run it on the phone
rem over USB with its log: quick tests without waiting for GitHub Actions.
rem
rem   tools\android_build.bat          get the latest code (git pull), build,
rem                                    install, start and show the log
rem   tools\android_build.bat nopull   build the code here as it is
rem
rem Needs Android Studio (Gradle fetches the NDK and CMake versions the app
rem uses if they're missing) and, so the APK installs over the GitHub builds,
rem keystore.dir in android\local.properties (tools\make_release_key.ps1 sets
rem it). The first build takes a few minutes; later ones only rebuild what
rem changed. The log is saved as tools\vboycolor-log.txt.

setlocal
cd /d "%~dp0.."

if /i not "%~1"=="nopull" (
    echo Getting the latest code...
    git pull --ff-only
    if errorlevel 1 (
        echo.
        echo git pull didn't work - changes of your own here? Build them as they
        echo are with: tools\android_build.bat nopull
        pause
        exit /b 1
    )
)

rem --- Java and the Android SDK: Android Studio's, unless set already ---
if not defined JAVA_HOME if exist "%ProgramFiles%\Android\Android Studio\jbr\bin\java.exe" set "JAVA_HOME=%ProgramFiles%\Android\Android Studio\jbr"
if not defined JAVA_HOME if exist "%LOCALAPPDATA%\Programs\Android Studio\jbr\bin\java.exe" set "JAVA_HOME=%LOCALAPPDATA%\Programs\Android Studio\jbr"
if not defined JAVA_HOME (
    echo Java wasn't found - install Android Studio, or set JAVA_HOME to a JDK 17.
    pause
    exit /b 1
)
if not defined ANDROID_HOME if exist "%LOCALAPPDATA%\Android\Sdk" set "ANDROID_HOME=%LOCALAPPDATA%\Android\Sdk"

findstr /b "keystore.dir" android\local.properties >nul 2>nul
if errorlevel 1 (
    echo.
    echo Note: no keystore.dir in android\local.properties - this APK is signed
    echo with this PC's debug key and won't install over the GitHub builds
    echo ^(uninstall VBoy Color from the phone first, or run
    echo tools\make_release_key.ps1^).
    echo.
)

echo.
echo Building...
pushd android
call gradlew.bat assembleRelease
if errorlevel 1 (
    popd
    echo.
    echo The build failed - see above.
    pause
    exit /b 1
)
popd

call "%~dp0android_debug.bat" "%CD%\android\app\build\outputs\apk\release\app-release.apk"
