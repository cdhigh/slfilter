@echo off
setlocal enabledelayedexpansion

rem ==============================================================================
rem Universal Build Script for Sprint-Layout 6.0 Macro Filter Plugin
rem Automatically detects available compilers or dispatches to specific scripts.
rem
rem Usage:
rem   build.cmd              (auto-detects and builds with best available compiler)
rem   build.cmd vs           (build with Visual Studio 2019/2022)
rem   build.cmd devcpp       (build with Dev-Cpp / MinGW 32-bit GCC)
rem   build.cmd tcc          (build with TinyCC)
rem   build.cmd vc6          (build with VC6)
rem   build.cmd cmake        (build with CMake)
rem ==============================================================================

cd /d "%~dp0"

if /i "%~1"=="vs"     goto :run_vs
if /i "%~1"=="msvc"   goto :run_vs
if /i "%~1"=="gcc"    goto :run_devcpp
if /i "%~1"=="devcpp" goto :run_devcpp
if /i "%~1"=="mingw"  goto :run_devcpp
if /i "%~1"=="tcc"    goto :run_tcc
if /i "%~1"=="vc6"    goto :run_vc6
if /i "%~1"=="cmake"  goto :run_cmake

if not "%~1"=="" (
    echo [ERROR] Unknown compiler option: %~1
    echo.
    echo Usage: build.cmd [vs ^| devcpp ^| tcc ^| vc6 ^| cmake]
    exit /b 1
)

rem ------------------------------------------------------------------------------
rem Auto-detect compiler
rem ------------------------------------------------------------------------------
echo [Auto-detecting available compiler...]

rem Check 1: VS2019 / VS2022
where cl.exe >nul 2>nul
if %errorlevel% equ 0 (
    echo Found active MSVC compiler in PATH.
    goto :run_vs
)
if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" (
    echo Found Visual Studio via vswhere.
    goto :run_vs
)
if exist "%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe" (
    echo Found Visual Studio via vswhere.
    goto :run_vs
)

rem Check 2: Dev-Cpp / MinGW 32-bit GCC
where gcc.exe >nul 2>nul
if %errorlevel% equ 0 (
    echo Found GCC in PATH.
    goto :run_devcpp
)
if exist "E:\Programer\C\Dev-Cpp\bin\gcc.exe" (
    echo Found Dev-Cpp MinGW 32-bit at E:\Programer\C\Dev-Cpp\bin.
    goto :run_devcpp
)

rem Check 3: TinyCC
where i386-win32-tcc.exe >nul 2>nul
if %errorlevel% equ 0 (
    echo Found TinyCC in PATH.
    goto :run_tcc
)
if exist "E:\Programer\C\tcc\i386-win32-tcc.exe" (
    echo Found TinyCC at E:\Programer\C\tcc.
    goto :run_tcc
)

rem Check 4: VC6
if exist "E:\Programer\C\VC6\VC98\BIN\cl.exe" (
    echo Found VC6 at E:\Programer\C\VC6.
    goto :run_vc6
)

rem Check 5: CMake
where cmake.exe >nul 2>nul
if %errorlevel% equ 0 (
    echo Found CMake in PATH.
    goto :run_cmake
)

echo [ERROR] No supported C compiler found in PATH or standard directories!
echo Supported compilers:
echo   - Visual Studio 2019 / 2022 (x86 C++ tools)
echo   - Dev-Cpp / MinGW 32-bit GCC
echo   - TinyCC (32-bit)
echo   - Visual C++ 6.0
echo   - CMake (with Win32 generator)
exit /b 1

:run_vs
call "%~dp0build_vs.cmd"
exit /b %errorlevel%

:run_devcpp
call "%~dp0build_devcpp.cmd"
exit /b %errorlevel%

:run_tcc
call "%~dp0build_tcc.cmd"
exit /b %errorlevel%

:run_vc6
call "%~dp0build_vc6.cmd"
exit /b %errorlevel%

:run_cmake
call "%~dp0build_cmake.cmd"
exit /b %errorlevel%
