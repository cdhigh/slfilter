@echo off
setlocal enabledelayedexpansion

rem ==============================================================================
rem Build script for Sprint-Layout Filter Plugin using TinyCC (TCC)
rem Target: 32-bit x86
rem ==============================================================================

cd /d "%~dp0"
if not exist dist mkdir dist

rem 1. Locate 32-bit TCC
set "TCC_BIN="
where i386-win32-tcc.exe >nul 2>nul
if %errorlevel% equ 0 (
    set "TCC_BIN=i386-win32-tcc.exe"
) else (
    where tcc.exe >nul 2>nul
    if %errorlevel% equ 0 (
        set "TCC_BIN=tcc.exe"
    ) else if exist "E:\Programer\C\tcc\i386-win32-tcc.exe" (
        set "TCC_BIN=E:\Programer\C\tcc\i386-win32-tcc.exe"
    ) else if exist "E:\Programer\C\tcc\tcc.exe" (
        set "TCC_BIN=E:\Programer\C\tcc\tcc.exe"
    )
)

if "%TCC_BIN%"=="" (
    echo [ERROR] TinyCC [i386-win32-tcc.exe or tcc.exe] not found!
    echo Please add TCC to PATH or install into E:\Programer\C\tcc\
    exit /b 1
)

rem 2. Locate windres.exe for resource compilation
set "WINDRES_BIN="
where windres.exe >nul 2>nul
if %errorlevel% equ 0 (
    set "WINDRES_BIN=windres.exe"
) else if exist "E:\Programer\C\Dev-Cpp\bin\windres.exe" (
    set "WINDRES_BIN=E:\Programer\C\Dev-Cpp\bin\windres.exe"
) else if exist "E:\Programer\C\RedPandaIDE\mingw64\bin\windres.exe" (
    set "WINDRES_BIN=E:\Programer\C\RedPandaIDE\mingw64\bin\windres.exe"
)

if not "%WINDRES_BIN%"=="" (
    for %%I in ("!WINDRES_BIN!") do set "PATH=%%~dpI;!PATH!"
    echo [1/3] Compiling version resource with windres...
    "!WINDRES_BIN!" -F pe-i386 -O coff src\version.rc -o dist\version_res.o || goto :err
    set "RES_OBJ=dist\version_res.o"
) else (
    echo [WARN] windres not found, building without version info resource...
    set "RES_OBJ="
)

echo [2/3] Compiling slfilter.c with TinyCC...
"!TCC_BIN!" -shared -O2 -o dist\version.dll src\slfilter.c !RES_OBJ! -luser32 -lkernel32 -lgdi32 || goto :err

rem 3. Fix exports (clean stdcall decorations)
where python.exe >nul 2>nul
if %errorlevel% equ 0 (
    echo [3/3] Normalizing export table with Python...
    python src\fix_exports.py dist\version.dll || goto :err
) else (
    echo [3/3] Python not found; skipping export name normalization.
)

rem Clean intermediate object
del /f /q dist\version_res.o >nul 2>nul

if exist ..\layout60.exe (
    copy /y dist\version.dll ..\version.dll >nul
    echo Build OK [TinyCC] - deployed to ..\version.dll
) else (
    echo Build OK [TinyCC] - output at dist\version.dll
)
exit /b 0

:err
echo [BUILD FAILED]
exit /b 1
