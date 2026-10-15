@echo off
rem ============================================================
rem  cpplib native 构建脚本（Windows）
rem  用法:  cpp\build-native.bat [windows-amd64] [-cuda] [-java HOME]
rem  产物:  <root>\native\windows-amd64\cpplib.dll   （CMake 直接输出到这里）
rem ============================================================
setlocal

set ROOT=%~dp0..
set PLAT=windows-amd64
set CUDA=OFF
set JAVA=

:args
if "%~1"=="" goto args_done
if /i "%~1"=="-cuda" (
  set CUDA=ON
  shift
  goto args
)
if /i "%~1"=="-java" (
  set JAVA=%~2
  shift
  shift
  goto args
)
set PLAT=%~1
shift
goto args
:args_done

set BUILD_DIR=%ROOT%\cpp\build\%PLAT%
set OUT_DIR=%ROOT%\native\%PLAT%

where cmake >nul 2>nul || ( echo [cpplib] cmake not found on PATH & exit /b 1 )

echo [cpplib] platform=%PLAT%  cuda=%CUDA%  out=%OUT_DIR%
if not exist "%OUT_DIR%" mkdir "%OUT_DIR%"

set EXTRA=
if not "%JAVA%"=="" set EXTRA=-DCPPLIB_JAVA_HOME="%JAVA%"

cmake -S "%ROOT%\cpp" -B "%BUILD_DIR%" -A x64 ^
  -DCPPLIB_OUTPUT_DIR="%OUT_DIR%" ^
  -DCPPLIB_USE_CUDA=%CUDA% %EXTRA%
if errorlevel 1 exit /b 1

cmake --build "%BUILD_DIR%" --config Release
if errorlevel 1 exit /b 1

rem 多配置生成器可能不理会 RUNTIME_OUTPUT_DIRECTORY_*，兜底从常见位置拷贝
if not exist "%OUT_DIR%\cpplib.dll" (
  if exist "%BUILD_DIR%\Release\cpplib.dll" copy /y "%BUILD_DIR%\Release\cpplib.dll" "%OUT_DIR%\" >nul
)
if not exist "%OUT_DIR%\cpplib.dll" (
  for /r "%BUILD_DIR%" %%f in (cpplib.dll) do @copy /y "%%f" "%OUT_DIR%\" >nul
)

echo.
echo [cpplib] done. artifacts in:
dir /b "%OUT_DIR%"

rem CUDA 构建会依赖 cudart64_*.dll，尝试一并拷贝，方便 Processing 直接加载
if /i "%CUDA%"=="ON" (
  if not "%CUDA_PATH%"=="" (
    copy /y "%CUDA_PATH%\bin\cudart64_*.dll" "%OUT_DIR%\" >nul 2>nul
    echo [cpplib] copied cudart from CUDA_PATH\bin
  )
)
endlocal
exit /b 0
