@echo off
setlocal
cd /d "%~dp0\.."

echo ================================================================
echo   WindL Unit Tests — Release Build
echo   %DATE% %TIME%
echo ================================================================
echo.

:: Step 1: Locate MSVC
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "VSINSTALL="

if exist "%VSWHERE%" (
    for /f "usebackq delims=" %%I in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
        set "VSINSTALL=%%I"
    )
)

if not defined VSINSTALL (
    for %%R in ("%ProgramFiles%\Microsoft Visual Studio" "%ProgramFiles(x86)%\Microsoft Visual Studio") do (
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
    echo [ERROR] Cannot find Visual Studio with C++ tools.
    echo Please install VS 2022 with "Desktop development with C++" workload.
    exit /b 1
)

echo [INFO] Visual Studio: %VSINSTALL%
set "VCVARS=%VSINSTALL%\VC\Auxiliary\Build\vcvars64.bat"
call "%VCVARS%" >nul 2>&1
if errorlevel 1 (
    echo [ERROR] Failed to initialize MSVC environment
    exit /b 1
)

:: Step 2: Create output directories
if not exist "build\test\obj" mkdir "build\test\obj"

:: Step 3: Compile (Release /MD)
echo.
echo [BUILD] Compiling UnitTests_release.exe ...
cl.exe /EHsc /std:c++20 /Zc:__cplusplus /MD /O2 /Ob2 /Ot /Oi /GL /Gy ^
  /wd4251 /wd4275 /MP /W3 /permissive- /openmp /arch:AVX2 ^
  /FS /utf-8 /bigobj /Zm999 ^
  /DNDEBUG /DNOMINMAX /D_CRT_SECURE_NO_WARNINGS /DEIGEN_USE_MKL_ALL ^
  /nologo ^
  /I "include\eigen-3.4.1" ^
  /I "Project\OpenXLSX-master\OpenXLSX" ^
  /I "Project\OpenXLSX-master\build\OpenXLSX" ^
  /I "Project\spectra-master\include" ^
  /I "include" ^
  /I "src" ^
  /I "Test\Io\math" ^
  @"Test\test_sources.rsp" ^
  /Fe"build\test\UnitTests_release.exe" ^
  /Fo"build\test\obj\\" ^
  /Fd"build\test\obj\vc143_release.pdb" ^
  /link /SUBSYSTEM:CONSOLE /OPT:REF /OPT:ICF /LTCG /DEBUG:NONE ^
  /LIBPATH:"lib\Release_d" ^
  "project\googletest-main\build_md_release\lib\Release\gtest.lib" ^
  "project\googletest-main\build_md_release\lib\Release\gmock.lib" ^
  "lib\Release_d\fmt.lib" ^
  > "build\test\release_build.log" 2>&1

set BUILD_RESULT=%ERRORLEVEL%

echo.
echo ================================================================
if %BUILD_RESULT% EQU 0 (
    echo   BUILD SUCCEEDED
    echo ================================================================
    echo.
    echo [RUN] Executing WindL tests...
    echo.
    "build\test\UnitTests_release.exe" --gtest_filter="WindL*"
    echo.
    echo [RUN] All tests (regression)...
    echo.
    "build\test\UnitTests_release.exe"
) else (
    echo   BUILD FAILED (exit code: %BUILD_RESULT%)
    echo ================================================================
    echo.
    type "build\test\release_build.log"
)

exit /b %BUILD_RESULT%
