@echo off
setlocal EnableExtensions EnableDelayedExpansion

REM ---------------------------------------------------------------------------
REM NativeSharp -- bug triage.
REM
REM Builds and runs the eight reproducers in tests\bug_triage\ against the nsc
REM currently installed in C:\native-sharp, and reports which reported bugs are
REM still alive. Several were reported against compilers older than C1, C6, C7
REM and C8, so some may already be dead.
REM
REM Each case returns 42 when the compiler is healthy. Anything else is the bug:
REM a build/link failure means the bug is noisy, a wrong exit code means it is
REM silent, which is worse.
REM
REM   run_bug_triage.bat
REM
REM Does NOT rebuild the compiler and does NOT touch anything outside _logs\.
REM Read _logs\TRIAGE.txt ; full detail in _logs\10_triage.log.
REM
REM NOTE: keep CRLF line endings. cmd.exe resolves "call :label" by byte offset
REM assuming CRLF; with LF-only endings it silently skips calls.
REM ---------------------------------------------------------------------------

set "ROOT=%~dp0"
if "%ROOT:~-1%"=="\" set "ROOT=%ROOT:~0,-1%"
set "LOGDIR=%ROOT%\_logs"
set "BINDIR=%LOGDIR%\bin_triage"
set "REPORT=%LOGDIR%\TRIAGE.txt"
set "TLOG=%LOGDIR%\10_triage.log"
set "BT=tests\bug_triage"

if not exist "%LOGDIR%" mkdir "%LOGDIR%"
if not exist "%BINDIR%" mkdir "%BINDIR%"

set /a ALIVE=0
set /a FIXED=0

> "%REPORT%" echo NativeSharp -- bug triage
>> "%REPORT%" echo Started: %DATE% %TIME%
>> "%REPORT%" echo Compiler: C:\native-sharp\bin\nsc.exe
>> "%REPORT%" echo.
>> "%REPORT%" echo Each case returns 42 when healthy. See tests\bug_triage\README.md.
>> "%REPORT%" echo.

> "%TLOG%" echo === bug triage detail ===

echo Running bug triage ...

call :triage "%BT%\b1_this_copy\b1_this_copy.nsproj"                             b1_this_copy              "T local = this; guarda el puntero (SILENCIOSO)"
call :triage "%BT%\b2_operator_null\b2_operator_null.nsproj"                     b2_operator_null          "x == null dentro de operator == recursa"
call :triage "%BT%\b3_ctor_aggregate\b3_ctor_aggregate.nsproj"                   b3_ctor_aggregate         "ctor no recibe struct >8 bytes por valor"
call :triage "%BT%\b4_this_by_value\b4_this_by_value.nsproj"                     b4_this_by_value          "this como argumento por valor"
call :triage "%BT%\b5_ctor_owned_field_array\b5_ctor_owned_field_array.nsproj"   b5_ctor_owned_field_array "new V3(...) a arreglo owned de campo"
call :triage "%BT%\b6_delegate_projectref\app\b6_delegate_projectref.nsproj"     b6_delegate_projectref    "extern C delegate no cruza ProjectReference"
call :triage "%BT%\b7_duplicate_extern_c\b7_duplicate_extern_c.nsproj"           b7_duplicate_extern_c     "mismo simbolo C en dos clases"
call :triage "%BT%\b8_shift_folding\b8_shift_folding.nsproj"                     b8_shift_folding          "1u << 28 pliega a double"

>> "%REPORT%" echo.
>> "%REPORT%" echo Vivos:     %ALIVE%
>> "%REPORT%" echo Arreglados:%FIXED%
>> "%REPORT%" echo Finished: %DATE% %TIME%

echo.
echo ---------------------------------------------------------------
type "%REPORT%"
echo ---------------------------------------------------------------
echo.
echo Detalle en: %TLOG%
exit /b 0

REM ===========================================================================
REM :triage  <.nsproj path, relative to root>  <label>  <one-line description>
REM ===========================================================================
:triage
set "TPROJ=%ROOT%\%~1"
set "TNAME=%~2"
set "TDESC=%~3"

if not exist "%TPROJ%" (
    >> "%REPORT%" echo [ ?? ] %TNAME% -- fixture no encontrado: %TPROJ%
    exit /b 0
)

for %%O in (O0 O2) do (
    set "NSC_OPT=-%%O"
    set "TEXE=%BINDIR%\%TNAME%_%%O.exe"
    >> "%TLOG%" echo.
    >> "%TLOG%" echo ============================================================
    >> "%TLOG%" echo --- %TNAME% -%%O : %TDESC%
    >> "%TLOG%" echo ============================================================
    call "%ROOT%\tools\build_project.cmd" "%TPROJ%" "!TEXE!" >> "%TLOG%" 2>&1
    if errorlevel 1 (
        call :alive "%TNAME% -%%O" "no compila/linkea -- %TDESC%"
    ) else (
        "!TEXE!" >> "%TLOG%" 2>&1
        set /a TRC=!ERRORLEVEL!
        >> "%TLOG%" echo [exit !TRC!]
        if "!TRC!"=="42" (
            call :fixed "%TNAME% -%%O" "%TDESC%"
        ) else (
            call :alive "%TNAME% -%%O" "exit !TRC! en vez de 42 -- %TDESC%"
        )
    )
)
exit /b 0

:alive
>> "%REPORT%" echo [VIVO] %~1 -- %~2
set /a ALIVE+=1
exit /b 0

:fixed
>> "%REPORT%" echo [ OK ] %~1 -- ARREGLADO (%~2)
set /a FIXED+=1
exit /b 0
