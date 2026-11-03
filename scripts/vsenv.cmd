@echo off
rem Loads the MSVC environment (vcvars64) and runs the supplied command.
rem Needed because the Ninja generator requires cl.exe/link.exe and LIB/INCLUDE
rem in the environment, and VS Code does not set them by itself.
rem Usage: scripts\vsenv.cmd cmake --preset relwithdebinfo

setlocal

if not defined VSWHERE set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo [vsenv] vswhere.exe not found: "%VSWHERE%" 1>&2
    exit /b 1
)

set "VSINSTALL="
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -prerelease -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINSTALL=%%i"
if not defined VSINSTALL (
    echo [vsenv] Visual Studio with C++ toolset not found 1>&2
    exit /b 1
)

set "VCVARS=%VSINSTALL%\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" (
    echo [vsenv] vcvars64.bat not found: "%VCVARS%" 1>&2
    exit /b 1
)

rem vcvars searches for vswhere on PATH; add its directory to avoid warnings.
for %%d in ("%VSWHERE%") do set "PATH=%%~dpd;%PATH%"

call "%VCVARS%" >nul
if errorlevel 1 (
    echo [vsenv] vcvars64.bat failed 1>&2
    exit /b 1
)

rem Otherwise cl.exe message language depends on the console code page: the /showIncludes
rem prefix that CMake writes to rules.ninja during configuration stops matching, and Ninja
rem does not track header dependencies. Always use English.
set "VSLANG=1033"

%*
exit /b %ERRORLEVEL%
