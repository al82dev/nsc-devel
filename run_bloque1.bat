@echo off
setlocal EnableExtensions

rem =====================================================================================
rem Bloque 1 -- IDs 123 (`stackalloc` hardening) y 89 (`sbyte`).
rem Doc: mkdown\84_bloque1_stackalloc_y_sbyte.md
rem
rem Orden, y no es arbitrario:
rem   1. Build con warnings-as-errors. El ID 89 anade un enumerador a TokenKind y una rama a
rem      cada switch que lo recorre; las dos funciones `...NodeKindName` no tienen `default`,
rem      asi que un `case` que falte se caza AQUI con -Wswitch y no tres pasos despues.
rem   2. nsc.cc1.integer-evaluator por separado: la tabla de ancho y signo del evaluador es
rem      una de las cinco que el ID 89 toca, y si discrepa de la del clasificador, discrepa
rem      ahi.
rem   3. nsc.cc1.correctness por separado: lleva las fixtures c18 y c19, las cinco pruebas
rem      negativas del 123, las cuatro del 89, y las aserciones de conversion.
rem   4. La suite entera. Es la unica red que hay.
rem
rem CRLF OBLIGATORIO en este archivo.
rem =====================================================================================

set "ROOT=%~dp0"
if "%ROOT:~-1%"=="\" set "ROOT=%ROOT:~0,-1%"
set "BUILDDIR=%ROOT%\build-clang"
set "LOGDIR=%ROOT%\_logs\bloque1"

if not exist "%LOGDIR%" mkdir "%LOGDIR%"

echo.
echo === 1/4  Compilando con warnings-as-errors =======================================
cmake --build "%BUILDDIR%" --parallel -- -k 0 > "%LOGDIR%\01_build.log" 2>&1
if errorlevel 1 goto :build_failed

echo.
echo === 2/4  El evaluador entero tipado =============================================
ctest --test-dir "%BUILDDIR%" -R "nsc.cc1.integer-evaluator" --output-on-failure > "%LOGDIR%\02_integer_evaluator.log" 2>&1
set RC_EVAL=%errorlevel%
type "%LOGDIR%\02_integer_evaluator.log"

echo.
echo === 3/4  Correctness CC1: c18 y c19 nuevas ======================================
ctest --test-dir "%BUILDDIR%" -R "nsc.cc1.correctness" --output-on-failure > "%LOGDIR%\03_cc1_correctness.log" 2>&1
set RC_CC1=%errorlevel%
type "%LOGDIR%\03_cc1_correctness.log"

echo.
echo === 4/4  La suite entera ========================================================
ctest --test-dir "%BUILDDIR%" --output-on-failure > "%LOGDIR%\04_suite.log" 2>&1
set RC_ALL=%errorlevel%

echo.
echo =================================================================================
if "%RC_EVAL%"=="0" (echo   integer-evaluator .... VERDE) else (echo   integer-evaluator .... ROJO)
if "%RC_CC1%"=="0"  (echo   cc1.correctness ...... VERDE) else (echo   cc1.correctness ...... ROJO)
if "%RC_ALL%"=="0"  (echo   suite completa ....... VERDE) else (echo   suite completa ....... ROJO)
echo.
echo   Logs en %LOGDIR%
echo =================================================================================
echo.
findstr /C:"tests passed" /C:"tests failed" /C:"Total Test time" "%LOGDIR%\04_suite.log"
echo.
echo Fallos, si los hubo:
findstr /C:"(Failed)" /C:"(Timeout)" "%LOGDIR%\04_suite.log"
echo.
echo NOTA: las fixtures c18 y c19 se COMPILAN aqui; para EJECUTARLAS en Windows con su
echo codigo de salida real, correr run_windows_checklist.bat despues.
goto :end

:build_failed
echo.
echo   El build fallo. Ver %LOGDIR%\01_build.log
echo   Ninja va con -k 0: arriba estan TODOS los archivos que fallaron, no solo el primero.
findstr /C:"error" "%LOGDIR%\01_build.log"

:end
endlocal
