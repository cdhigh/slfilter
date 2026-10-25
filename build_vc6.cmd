@echo off
setlocal enabledelayedexpansion

rem ==============================================================================
rem Build script for Sprint-Layout Filter Plugin using Visual C++ 6.0
rem Target: 32-bit x86
rem ==============================================================================

cd /d "%~dp0"
if not exist dist mkdir dist

if not defined MSVCDir (
    if exist "E:\Programer\C\VC6\VC98" (
        set "MSVCDir=E:\Programer\C\VC6\VC98"
        set "PATH=!MSVCDir!\BIN;E:\Programer\C\VC6\Common\MSDev98\Bin;!PATH!"
        set "INCLUDE=!MSVCDir!\INCLUDE;!MSVCDir!\MFC\INCLUDE;!INCLUDE!"
        set "LIB=!MSVCDir!\LIB;!MSVCDir!\MFC\LIB;!LIB!"
    )
)

where cl.exe >nul 2>nul
if %errorlevel% neq 0 (
    echo [ERROR] VC6 cl.exe not found!
    exit /b 1
)

echo [1/2] Compiling version resource with rc...
rc /fo dist\version_vc6.res src\version.rc || goto :err

echo [2/2] Compiling slfilter.c with VC6...
cl /nologo /LD /O2 /MD /Fo"dist\slfilter_vc6.obj" src\slfilter.c dist\version_vc6.res /link /DEF:src\version.def /IMPLIB:dist\version_vc6.lib /OUT:dist\version.dll user32.lib kernel32.lib gdi32.lib || goto :err

del /f /q dist\version_vc6.res dist\slfilter_vc6.obj dist\version_vc6.exp dist\version_vc6.lib >nul 2>nul

if exist ..\layout60.exe (
    copy /y dist\version.dll ..\version.dll >nul
    echo Build OK [VC6] - deployed to ..\version.dll
) else (
    echo Build OK [VC6] - output at dist\version.dll
)
exit /b 0

:err
echo [BUILD FAILED]
exit /b 1
