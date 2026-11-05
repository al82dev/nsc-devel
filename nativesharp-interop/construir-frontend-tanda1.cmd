@echo off
setlocal
set "TASK_ROOT=A:\vc2026-runtime"
set "TASK_TOOLSET=%TASK_ROOT%\toolsets\14.52.36815"
set "TASK_SDK=%TASK_ROOT%\windows-sdk\10.0.28000.0"
set "INCLUDE=%TASK_TOOLSET%\include;%TASK_SDK%\include\ucrt;%TASK_SDK%\include\shared;%TASK_SDK%\include\um;%TASK_SDK%\include\winrt;%TASK_TOOLSET%\dia\include"
set "LIB=%TASK_TOOLSET%\lib\x64;%TASK_SDK%\lib\ucrt\x64;%TASK_SDK%\lib\um\x64;%TASK_TOOLSET%\dia\lib\x64"
set "TEMP=A:\nsc-interop-build\temp"
set "TMP=%TEMP%"
set "TASK_TARGET=%~1"
if "%TASK_TARGET%"=="" set "TASK_TARGET=nsc_frontend"
set "TASK_MODE=%~2"
if "%TASK_MODE%"=="" set "TASK_MODE=OFF"
if /I not "%TASK_MODE%"=="ON" if /I not "%TASK_MODE%"=="OFF" exit /b 2
cmake --build A:/nsc-interop-build/tanda1-%TASK_MODE% --target %TASK_TARGET% --parallel 4
exit /b %ERRORLEVEL%
