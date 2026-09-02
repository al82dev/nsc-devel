@echo off
setlocal EnableExtensions

rem =====================================================================================
rem Recompila y corre SOLO nsc.cc1.correctness, que es el unico test rojo que queda.
rem Deja _logs\c17.log y lo imprime. No borra nada, no instala nada.
rem CRLF OBLIGATORIO en este archivo.
rem =====================================================================================

set "ROOT=%~dp0"
if "%ROOT:~-1%"=="\" set "ROOT=%ROOT:~0,-1%"
set "BUILDDIR=%ROOT%\build-clang"
set "LOGDIR=%ROOT%\_logs"
set "OUT=%LOGDIR%\c17.log"

if not exist "%LOGDIR%" mkdir "%LOGDIR%"

echo [1/2] Recompilando ...
cmake --build "%BUILDDIR%" --parallel -- -k 0 > "%OUT%" 2>&1
if errorlevel 1 goto :build_failed

echo [2/2] Corriendo nsc.cc1.correctness ...
ctest --test-dir "%BUILDDIR%" -R "nsc.cc1.correctness" --output-on-failure >> "%OUT%" 2>&1
set RC=%errorlevel%

echo.
type "%OUT%"
echo.
if "%RC%"=="0" echo   nsc.cc1.correctness .... VERDE
if not "%RC%"=="0" echo   nsc.cc1.correctness .... ROJO
echo   Log completo en %OUT%
goto :end

:build_failed
echo.
echo   El build fallo. Ver %OUT%
findstr /C:"error" "%OUT%"

:end
endlocal
