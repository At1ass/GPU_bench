@echo off
setlocal enabledelayedexpansion

set "PROJECT_DIR=%~dp0.."
if not defined BUILD_TYPE set "BUILD_TYPE=Release"

if "%~1"=="" goto :usage
if "%~1"=="help"   goto :usage
if "%~1"=="--help" goto :usage
if "%~1"=="mingw"  goto :build_mingw
if "%~1"=="msvc"   goto :build_msvc
if "%~1"=="clean"  goto :clean
goto :usage

:usage
echo Usage: build.bat ^<target^>
echo.
echo Targets:
echo   mingw         Native build with MinGW (g++ must be in PATH)
echo   msvc          Native build with MSVC (Visual Studio must be installed)
echo   clean         Remove build directory
echo.
echo Environment:
echo   BUILD_TYPE    CMake build type (default: Release)
echo   SDL2_DIR      Path to SDL2 CMake config (for MSVC)
exit /b 1

:build_mingw
echo === Building with MinGW (%BUILD_TYPE%) ===

where g++ >nul 2>&1
if errorlevel 1 (
    echo Error: g++ not found in PATH.
    echo Install MinGW-w64 and add its bin directory to PATH.
    echo.
    echo MSYS2:  pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-SDL2
    echo         Run from MSYS2 MinGW shell, not MSYS shell.
    exit /b 1
)

set "BUILD_DIR=%PROJECT_DIR%\build_win"
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
cd /d "%BUILD_DIR%"

cmake "%PROJECT_DIR%" -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=%BUILD_TYPE%
if errorlevel 1 exit /b 1

cmake --build . -- -j%NUMBER_OF_PROCESSORS%
if errorlevel 1 exit /b 1

echo.
echo Done: %BUILD_DIR%\gpu_benchmark.exe
exit /b 0

:build_msvc
echo === Building with MSVC (%BUILD_TYPE%) ===

where cl >nul 2>&1
if errorlevel 1 (
    echo Error: cl.exe not found in PATH.
    echo Run this script from a Visual Studio Developer Command Prompt,
    echo or use "vcvarsall.bat x64" to set up the environment first.
    exit /b 1
)

set "BUILD_DIR=%PROJECT_DIR%\build_win"
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
cd /d "%BUILD_DIR%"

cmake "%PROJECT_DIR%" -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=%BUILD_TYPE%
if errorlevel 1 (
    echo.
    echo If SDL2 was not found, set SDL2_DIR to the path containing SDL2Config.cmake:
    echo   set SDL2_DIR=C:\path\to\SDL2\cmake
    echo   build.bat msvc
    exit /b 1
)

cmake --build .
if errorlevel 1 exit /b 1

echo.
echo Done: %BUILD_DIR%\gpu_benchmark.exe
echo Note: copy SDL2.dll next to the exe before running.
exit /b 0

:clean
echo Cleaning build directory...
if exist "%PROJECT_DIR%\build_win" rmdir /s /q "%PROJECT_DIR%\build_win"
echo Done.
exit /b 0
