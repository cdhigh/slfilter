@echo off
setlocal enabledelayedexpansion

rem ==============================================================================
rem Build script for Sprint-Layout Filter Plugin using MSVC (VS2019 / VS2022)
rem Target: 32-bit x86 (Sprint-Layout 6.0 is a 32-bit application)
rem ==============================================================================

cd /d "%~dp0"
if not exist dist mkdir dist

rem 1. Check if cl.exe is already available in PATH
where cl.exe >nul 2>nul
if %errorlevel% equ 0 goto :compile

rem 2. Try locating vswhere.exe
set "VSWHERE="
if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" (
    set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
) else if exist "%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe" (
    set "VSWHERE=%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe"
)

if defined VSWHERE (
    for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
        set "VS_PATH=%%i"
    )
)

if defined VS_PATH (
    if exist "!VS_PATH!\VC\Auxiliary\Build\vcvars32.bat" (
        call "!VS_PATH!\VC\Auxiliary\Build\vcvars32.bat" >nul
        goto :compile
    )
    if exist "!VS_PATH!\VC\Auxiliary\Build\vcvarsall.bat" (
        call "!VS_PATH!\VC\Auxiliary\Build\vcvarsall.bat" x86 >nul
        goto :compile
    )
)

rem 3. Fallback checks for common Visual Studio default directories
for %%p in (
    "%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat"
    "%ProgramFiles%\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars32.bat"
    "%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars32.bat"
    "%ProgramFiles%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars32.bat"
    "%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars32.bat"
    "%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvars32.bat"
    "%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Enterprise\VC\Auxiliary\Build\vcvars32.bat"
    "%ProgramFiles(x86)%\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars32.bat"
    "%ProgramFiles(x86)%\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvars32.bat"
) do (
    if exist %%p (
        call %%p >nul
        goto :compile
    )
)

echo [ERROR] MSVC environment not found!
echo Please run this script inside 'x86 Native Tools Command Prompt for VS 2019/2022'
echo or ensure Visual Studio (with C++ x86/x64 tools) is installed.
exit /b 1

:compile
echo [1/2] Compiling version resource...
rc /nologo /fo dist\version_vs.res src\version.rc || goto :err

echo [2/2] Compiling slfilter.c with MSVC (x86)...
cl /nologo /LD /O2 /MD /Fo"dist\slfilter_vs.obj" src\slfilter.c dist\version_vs.res /link /DEF:src\version.def /IMPLIB:dist\version_vs.lib /OUT:dist\version.dll user32.lib kernel32.lib gdi32.lib || goto :err

rem Clean up intermediate build artifacts
del /f /q dist\version_vs.res dist\slfilter_vs.obj dist\version_vs.exp dist\version_vs.lib >nul 2>nul

if exist ..\layout60.exe (
    copy /y dist\version.dll ..\version.dll >nul
    echo Build OK [MSVC x86] - deployed to ..\version.dll
) else (
    echo Build OK [MSVC x86] - output at dist\version.dll
)
exit /b 0

:err
echo [BUILD FAILED]
exit /b 1
