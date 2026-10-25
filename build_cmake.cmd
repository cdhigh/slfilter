@echo off
setlocal enabledelayedexpansion

rem ==============================================================================
rem Build script for Sprint-Layout Filter Plugin using CMake
rem Target: 32-bit x86
rem ==============================================================================

cd /d "%~dp0"
if not exist dist mkdir dist

where cmake.exe >nul 2>nul
if %errorlevel% neq 0 (
    echo [ERROR] CMake not found in PATH!
    echo Please install CMake from https://cmake.org/download/ and add it to your PATH.
    exit /b 1
)

echo [1/2] Configuring CMake project (Win32 / 32-bit)...
cmake -B build -A Win32 || goto :err

echo [2/2] Building Release target...
cmake --build build --config Release || goto :err

set "OUTPUT_DLL="
if exist build\Release\version.dll set "OUTPUT_DLL=build\Release\version.dll"
if exist build\version.dll set "OUTPUT_DLL=build\version.dll"

if not defined OUTPUT_DLL (
    echo [ERROR] Build succeeded but version.dll output was not found!
    exit /b 1
)

copy /y "!OUTPUT_DLL!" dist\version.dll >nul

if exist ..\layout60.exe (
    copy /y dist\version.dll ..\version.dll >nul
    echo Build OK [CMake] - deployed to ..\version.dll
) else (
    echo Build OK [CMake] - output at dist\version.dll
)
exit /b 0

:err
echo [BUILD FAILED]
exit /b 1
