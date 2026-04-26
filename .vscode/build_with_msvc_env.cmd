@echo off
setlocal

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "VSINSTALL="

if exist "%VSWHERE%" (
    for /f "usebackq delims=" %%I in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
        set "VSINSTALL=%%I"
    )
)

if not defined VSINSTALL (
    for %%R in ("%ProgramFiles%\Microsoft Visual Studio" "%ProgramFiles(x86)%\Microsoft Visual Studio" "D:\Program Files\Microsoft Visual Studio") do (
        if not defined VSINSTALL if exist "%%~R" (
            for /d %%D in ("%%~R\*") do (
                if not defined VSINSTALL if exist "%%~fD\VC\Auxiliary\Build\vcvars64.bat" (
                    set "VSINSTALL=%%~fD"
                )
            )
        )
    )
)

if not defined VSINSTALL (
    echo [build_with_msvc_env] ERROR: Cannot locate a Visual Studio installation with C++ tools.
    exit /b 1
)

set "VCVARS=%VSINSTALL%\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" (
    echo [build_with_msvc_env] ERROR: vcvars64.bat not found: "%VCVARS%"
    exit /b 1
)

call "%VCVARS%" >nul
if errorlevel 1 (
    echo [build_with_msvc_env] ERROR: Failed to initialize MSVC environment.
    exit /b 1
)

cl.exe %*
exit /b %ERRORLEVEL%
