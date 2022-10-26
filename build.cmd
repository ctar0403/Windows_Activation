@echo off
setlocal EnableExtensions EnableDelayedExpansion

cd /d "%~dp0"

set "CONFIG=Release"
if not "%~1"=="" set "CONFIG=%~1"
if /I not "%CONFIG%"=="Release" if /I not "%CONFIG%"=="Debug" (
    echo [ERROR] Configuration must be Release or Debug.
    goto :failed
)

set "CMAKE_EXE="
for /f "delims=" %%I in ('where cmake.exe 2^>nul') do if not defined CMAKE_EXE set "CMAKE_EXE=%%I"

if not defined CMAKE_EXE (
    set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
    if exist "!VSWHERE!" (
        for /f "usebackq delims=" %%I in (`"!VSWHERE!" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VS_INSTALL=%%I"
        if defined VS_INSTALL if exist "!VS_INSTALL!\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" (
            set "CMAKE_EXE=!VS_INSTALL!\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
        )
    )
)

if not defined CMAKE_EXE if exist "%ProgramFiles%\CMake\bin\cmake.exe" set "CMAKE_EXE=%ProgramFiles%\CMake\bin\cmake.exe"

if not defined CMAKE_EXE (
    set "GXX_EXE="
    for /f "delims=" %%I in ('where g++.exe 2^>nul') do if not defined GXX_EXE set "GXX_EXE=%%I"
    if not defined GXX_EXE if exist "C:\PROGRA~2\Embarcadero\Dev-Cpp\TDM-GCC-64\bin\g++.exe" set "GXX_EXE=C:\PROGRA~2\Embarcadero\Dev-Cpp\TDM-GCC-64\bin\g++.exe"
    if defined GXX_EXE goto :build_mingw
    echo [ERROR] No supported compiler toolchain was found.
    echo Install Visual Studio 2022 with "Desktop development with C++" and CMake tools.
    goto :failed
)

:build_msvc
for %%I in ("!CMAKE_EXE!") do set "CMAKE_BIN=%%~dpI"
set "BUILD_DIR=build\vs2022-x64"

echo [1/4] Configuring WinActivate Manager ^(!CONFIG!, x64^)...
"!CMAKE_EXE!" -S . -B "!BUILD_DIR!" -G "Visual Studio 17 2022" -A x64 -DBUILD_TESTING=ON
if errorlevel 1 goto :failed

echo [2/4] Building...
"!CMAKE_EXE!" --build "!BUILD_DIR!" --config "!CONFIG!" --parallel
if errorlevel 1 goto :failed

echo [3/4] Running safe, mock-backed tests...
"!CMAKE_BIN!ctest.exe" --test-dir "!BUILD_DIR!" -C "!CONFIG!" --output-on-failure
if errorlevel 1 goto :failed

echo [4/4] Preparing test executable...
if not exist "dist" mkdir "dist"
copy /Y "!BUILD_DIR!\!CONFIG!\WinActivate.exe" "dist\WinActivate.exe" >nul
if errorlevel 1 goto :failed

echo.
echo Build complete:
echo   %CD%\dist\WinActivate.exe
echo.
echo This preview reads Windows licensing status only. Activation actions are disabled.
goto :success

:build_mingw
for %%I in ("!GXX_EXE!") do set "GXX_EXE=%%~sI"
for %%I in ("!GXX_EXE!") do set "MINGW_BIN=%%~dpI"
set "WINDRES_EXE=!MINGW_BIN!windres.exe"
set "BUILD_DIR=build\mingw-x64"
set "OPT_FLAGS=-O2 -s"
if /I "!CONFIG!"=="Debug" set "OPT_FLAGS=-O0 -g"
if not exist "!BUILD_DIR!" mkdir "!BUILD_DIR!"

echo [INFO] Visual Studio/CMake was not found; using the MinGW preview fallback.
echo [1/4] Compiling safe unit tests...
"!GXX_EXE!" -std=c++2a -Wall -Wextra -Wpedantic -DUNICODE -D_UNICODE -Isrc -Itests tests\TestsMain.cpp tests\KeyValidationTests.cpp tests\OsDetectionTests.cpp tests\ProductSelectionTests.cpp tests\ErrorMappingTests.cpp tests\MockBackendTests.cpp src\common\KeyValidation.cpp src\common\FormatError.cpp src\core\OsDetector.cpp src\core\ProductSelection.cpp src\core\ActivationWorkflow.cpp src\core\MockLicensingBackend.cpp -o "!BUILD_DIR!\WinActivateTests.exe"
if errorlevel 1 goto :failed

echo [2/4] Running safe, mock-backed tests...
"!BUILD_DIR!\WinActivateTests.exe"
if errorlevel 1 goto :failed

echo [3/4] Compiling resources and interface...
"!WINDRES_EXE!" -DUNICODE -D_UNICODE -Isrc\gui src\gui\Resources.rc -O coff -o "!BUILD_DIR!\Resources.o"
if errorlevel 1 goto :failed
"!GXX_EXE!" -std=c++2a !OPT_FLAGS! -DUNICODE -D_UNICODE -D_WIN32_WINNT=0x0601 -Isrc -municode -mwindows src\gui\Main.cpp src\gui\MainWindow.cpp src\common\KeyValidation.cpp src\common\FormatError.cpp src\core\WmiConnection.cpp src\core\LicenseReader.cpp src\core\OsDetector.cpp src\core\ProductSelection.cpp src\core\ActivationWorkflow.cpp src\core\MockLicensingBackend.cpp "!BUILD_DIR!\Resources.o" -lcomctl32 -lshell32 -lgdi32 -lole32 -loleaut32 -lwbemuuid -o "!BUILD_DIR!\WinActivate.exe"
if errorlevel 1 goto :failed

echo [4/4] Preparing test executable...
if not exist "dist" mkdir "dist"
copy /Y "!BUILD_DIR!\WinActivate.exe" "dist\WinActivate.exe" >nul
if errorlevel 1 goto :failed
echo.
echo MinGW preview build complete:
echo   %CD%\dist\WinActivate.exe
echo.
echo Use the Visual Studio/CMake path for authoritative release validation.
goto :success

:failed
echo.
echo Build failed. Review the error above.
if /I not "%~2"=="--no-pause" pause
exit /b 1

:success
if /I not "%~2"=="--no-pause" pause
exit /b 0

