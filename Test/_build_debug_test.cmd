@echo off
setlocal
cd /d "%~dp0\.."

echo ================================================================
echo   Unit Tests — Debug Build (matching .vscode/tasks.json)
echo   %DATE% %TIME%
echo ================================================================

call ".vscode\build_with_msvc_env.cmd" ^
  /EHsc /std:c++20 /Zc:__cplusplus ^
  /MDd ^
  /Zi /Od /Ob0 /RTC1 ^
  /wd4251 /wd4275 ^
  /MP /W3 /permissive- ^
  /FS /utf-8 /bigobj /Zm999 ^
  /DNDEBUG /DNOMINMAX /D_CRT_SECURE_NO_WARNINGS /DEIGEN_USE_MKL_ALL ^
  /nologo ^
  /I "include\eigen-3.4.1" ^
  /I "Project\OpenXLSX-master\OpenXLSX" ^
  /I "Project\OpenXLSX-master\build\OpenXLSX" ^
  /I "Project\spectra-master\include" ^
  /I "C:\Program Files (x86)\Intel\oneAPI\mkl\latest\include" ^
  /I "include" ^
  /I "src" ^
  /I "Test\Io\math" ^
  @"Test\test_sources.rsp" ^
  /Fe"build\test\UnitTests_debug.exe" ^
  /Fo"build\test\debug_obj\\" ^
  /Fd"build\test\debug_obj\vc143.pdb" ^
  /link ^
  /SUBSYSTEM:CONSOLE ^
  /DEBUG:FULL ^
  /LIBPATH:"C:\Program Files (x86)\Intel\oneAPI\mkl\latest\lib" ^
  /LIBPATH:"C:\Program Files (x86)\Intel\oneAPI\compiler\latest\lib" ^
  /LIBPATH:"lib\Debug_d" ^
  mkl_intel_lp64_dll.lib ^
  mkl_intel_thread_dll.lib ^
  mkl_core_dll.lib ^
  libiomp5md.lib ^
  fmtd.lib ^
  gtest.lib ^
  gmock.lib ^
  OpenXLSXd.lib ^
  > "build\test\debug_build.log" 2>&1

set BUILD_RESULT=%ERRORLEVEL%

echo.
echo ================================================================
if %BUILD_RESULT% EQU 0 (
    echo   DEBUG BUILD SUCCEEDED
    echo ================================================================
    echo.
    echo [RUN] WindL debug tests...
    "build\test\UnitTests_debug.exe" --gtest_filter="WindL*"
    echo.
    echo [RUN] All debug tests...
    "build\test\UnitTests_debug.exe"
) else (
    echo   DEBUG BUILD FAILED (exit code: %BUILD_RESULT%)
    echo ================================================================
    echo Last 30 lines of build log:
    powershell -Command "Get-Content 'build\test\debug_build.log' | Select-Object -Last 30"
)

exit /b %BUILD_RESULT%
