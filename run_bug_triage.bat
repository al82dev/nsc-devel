@echo off
setlocal EnableExtensions

REM ---------------------------------------------------------------------------
REM NativeSharp -- bug triage.
REM
REM Builds and runs the reproducers in tests\bug_triage\ against the nsc
REM currently installed in C:\native-sharp, and reports which reported bugs are
REM still alive. Several were reported against compilers older than C1, C6, C7
REM and C8, so some may already be dead.
REM
REM Each case returns 42 when the compiler is healthy. Anything else is the bug:
REM a build or link failure means the bug is noisy, a wrong exit code means it
REM is silent, which is worse.
REM
REM   run_bug_triage.bat
REM
REM Does NOT rebuild the compiler and does NOT touch anything outside _logs\.
REM Read _logs\TRIAGE.txt ; full detail in _logs\10_triage.log.
REM
REM STYLE NOTES, both learned the hard way on this repo:
REM   * Keep CRLF line endings. cmd.exe resolves "call :label" by byte offset
REM     assuming CRLF; with LF-only endings it silently skips calls and runs
REM     labels out of order.
REM   * No parenthesized for/if blocks around the work, and no free text in
REM     call arguments. Characters like ( ) < > ; = break block parsing even
REM     inside double quotes. Every case is two flat calls instead.
REM ---------------------------------------------------------------------------

set "ROOT=%~dp0"
if "%ROOT:~-1%"=="\" set "ROOT=%ROOT:~0,-1%"
REM NSC_LOG_DIR: `all.bat` corre las dos pasadas en directorios separados y este script
REM escribe en el de la pasada que lo llamo. Sin la variable, _logs\ como siempre.
if defined NSC_LOG_DIR (set "LOGDIR=%ROOT%\%NSC_LOG_DIR%") else (set "LOGDIR=%ROOT%\_logs")
set "BINDIR=%LOGDIR%\bin_triage"
set "REPORT=%LOGDIR%\TRIAGE.txt"
set "TLOG=%LOGDIR%\10_triage.log"
set "BT=%ROOT%\tests\bug_triage"
if not defined NS_ROOT set "NS_ROOT=C:\native-sharp"
if not defined NSC_EXE set "NSC_EXE=%NS_ROOT%\bin\nsc.exe"

if not exist "%LOGDIR%" mkdir "%LOGDIR%"
if not exist "%BINDIR%" mkdir "%BINDIR%"

set /a ALIVE=0
set /a FIXED=0

> "%REPORT%" echo NativeSharp -- bug triage
>> "%REPORT%" echo Started: %DATE% %TIME%
>> "%REPORT%" echo Compiler: %NSC_EXE%
>> "%REPORT%" echo.
>> "%REPORT%" echo Cada caso devuelve 42 cuando el compilador esta sano.
>> "%REPORT%" echo Ver tests\bug_triage\README.md para que mide cada uno.
>> "%REPORT%" echo.

> "%TLOG%" echo === bug triage detail ===

echo Running bug triage ...

call :triage "%BT%\b1_this_copy\b1_this_copy.nsproj" b1_this_copy
call :triage "%BT%\b2_operator_null\b2_operator_null.nsproj" b2_operator_null
call :triage "%BT%\b3_ctor_aggregate\b3_ctor_aggregate.nsproj" b3_ctor_aggregate
call :triage "%BT%\b4_this_by_value\b4_this_by_value.nsproj" b4_this_by_value
call :triage "%BT%\b5_ctor_owned_field_array\b5_ctor_owned_field_array.nsproj" b5_ctor_owned_field_array
call :triage "%BT%\b6_delegate_projectref\app\b6_delegate_projectref.nsproj" b6_delegate_projectref
call :triage "%BT%\b7_duplicate_extern_c\b7_duplicate_extern_c.nsproj" b7_duplicate_extern_c
call :triage "%BT%\b8_shift_folding\b8_shift_folding.nsproj" b8_shift_folding
call :triage "%BT%\b9_enum_estrecho_o2\b9_enum_estrecho_o2.nsproj" b9_enum_estrecho_o2

>> "%REPORT%" echo.
>> "%REPORT%" echo Vivos: %ALIVE%
>> "%REPORT%" echo Arreglados: %FIXED%
>> "%REPORT%" echo Finished: %DATE% %TIME%

echo.
echo ---------------------------------------------------------------
type "%REPORT%"
echo ---------------------------------------------------------------
echo.
echo Detalle en: %TLOG%

REM Salir distinto de cero si queda alguno vivo, para que run_windows_checklist.bat pueda
REM encadenarlo y no haya que leer el reporte a ojo.
if %ALIVE% GTR 0 exit /b 1
exit /b 0

REM ===========================================================================
REM :triage  <absolute .nsproj path>  <label>
REM ===========================================================================
:triage
set "TPROJ=%~1"
set "TNAME=%~2"

REM UNA FIXTURE QUE NO ESTA CUENTA COMO VIVA, y esto no es una sutileza: antes se imprimia
REM el aviso y se salia con exito sin tocar ALIVE, asi que borrar una carpeta --o escribir mal
REM su ruta al inscribirla-- daba "cero vivos" con el bug intacto. Un falso verde es peor que
REM un rojo: el rojo se investiga, el verde se cree.
if exist "%TPROJ%" goto :triage_run
>> "%REPORT%" echo [VIVO] %TNAME% -- fixture no encontrado, NO se midio
>> "%REPORT%" echo        %TPROJ%
set /a ALIVE+=1
exit /b 0

:triage_run
call :one "%TPROJ%" "%TNAME%" O0
call :one "%TPROJ%" "%TNAME%" O2
exit /b 0

REM ===========================================================================
REM :one  <absolute .nsproj path>  <label>  <O0|O2>
REM ===========================================================================
:one
set "OPROJ=%~1"
set "ONAME=%~2"
set "OOPT=%~3"
set "NSC_OPT=-%OOPT%"
set "OEXE=%BINDIR%\%ONAME%_%OOPT%.exe"

>> "%TLOG%" echo.
>> "%TLOG%" echo ============================================================
>> "%TLOG%" echo --- %ONAME% -%OOPT%
>> "%TLOG%" echo ============================================================

call "%ROOT%\tools\build_project.cmd" "%OPROJ%" "%OEXE%" >> "%TLOG%" 2>&1
if errorlevel 1 goto :one_build_failed

"%OEXE%" >> "%TLOG%" 2>&1
set "ORC=%ERRORLEVEL%"
>> "%TLOG%" echo [exit %ORC%]

if "%ORC%"=="42" goto :one_fixed

>> "%REPORT%" echo [VIVO] %ONAME% -%OOPT% -- exit %ORC%, se esperaba 42
set /a ALIVE+=1
exit /b 0

:one_build_failed
>> "%TLOG%" echo [build failed]
>> "%REPORT%" echo [VIVO] %ONAME% -%OOPT% -- no compila o no linkea
set /a ALIVE+=1
exit /b 0

:one_fixed
>> "%REPORT%" echo [ OK ] %ONAME% -%OOPT% -- ARREGLADO
set /a FIXED+=1
exit /b 0
