@echo off
setlocal enabledelayedexpansion

rem ==============================================================================
rem Build script for Sprint-Layout Filter Plugin using Dev-Cpp (MinGW 32-bit GCC)
rem Target: 32-bit x86
rem ==============================================================================

cd /d "%~dp0"
if not exist dist mkdir dist

rem Locate 32-bit MinGW GCC (PATH or Dev-Cpp)
where gcc.exe >nul 2>nul
if %errorlevel% neq 0 (
    if exist "E:\Programer\C\Dev-Cpp\bin\gcc.exe" (
        set "PATH=E:\Programer\C\Dev-Cpp\bin;!PATH!"
    ) else (
        echo [ERROR] 32-bit GCC not found in PATH or E:\Programer\C\Dev-Cpp\bin!
        exit /b 1
    )
)

echo [1/2] Compiling version resource with windres...
windres -O coff src\version.rc -o dist\version_devcpp.res || goto :err

echo [2/2] Compiling slfilter.c with GCC...
gcc -shared -O2 -s -Wl,--enable-stdcall-fixup -o dist\version.dll src\slfilter.c src\version.def dist\version_devcpp.res -luser32 -lkernel32 -lgdi32 || goto :err

del /f /q dist\version_devcpp.res >nul 2>nul

if exist ..\layout60.exe (
    copy /y dist\version.dll ..\version.dll >nul
    echo Build OK [GCC 32-bit] - deployed to ..\version.dll
) else (
    echo Build OK [GCC 32-bit] - output at dist\version.dll
)
exit /b 0

:err
echo [BUILD FAILED]
exit /b 1
