@echo off
setlocal EnableExtensions EnableDelayedExpansion

REM ---------------------------------------------------------------------------
REM NativeSharp -- real Windows execution checklist.
REM
REM Rebuilds and installs the compiler, runs ctest, then BUILDS AND RUNS every
REM interop contract fixture and every Stage 56 packaged example at -O0 and -O2,
REM comparing the real process exit code against the documented contract.
REM
REM This is the half of the test matrix that the sandbox rounds report as
REM "Wine unavailable -- execution skipped".
REM
REM   run_windows_checklist.bat             full run (rebuild + install + tests)
REM   run_windows_checklist.bat norebuild   skip the cmake rebuild
REM
REM Everything lands in _logs\. Read _logs\SUMMARY.txt first.
REM Nothing outside _logs\ is written.
REM
REM NOTE: this file must keep CRLF line endings. cmd.exe resolves "call :label"
REM by byte offset assuming CRLF; with LF-only endings it silently skips calls
REM and runs labels out of order.
REM ---------------------------------------------------------------------------

set "ROOT=%~dp0"
if "%ROOT:~-1%"=="\" set "ROOT=%ROOT:~0,-1%"
set "LOGDIR=%ROOT%\_logs"
set "BINDIR=%LOGDIR%\bin"
set "SUMMARY=%LOGDIR%\SUMMARY.txt"
set "FIXLOG=%LOGDIR%\03_fixtures.log"
set "EXLOG=%LOGDIR%\04_examples.log"

if not exist "%LOGDIR%" mkdir "%LOGDIR%"
if not exist "%BINDIR%" mkdir "%BINDIR%"

set /a PASSED=0
set /a FAILED=0

> "%SUMMARY%" echo NativeSharp -- Windows execution checklist
>> "%SUMMARY%" echo Started: %DATE% %TIME%
>> "%SUMMARY%" echo Root   : %ROOT%
>> "%SUMMARY%" echo.

> "%FIXLOG%" echo === interop contract fixtures ===
> "%EXLOG%" echo === packaged examples ===

REM --- 1. Rebuild + install ---------------------------------------------------
if /I "%~1"=="norebuild" goto :skip_build

echo [1/4] Rebuilding compiler and installing to C:\native-sharp ...
call "%ROOT%\build-all.bat" > "%LOGDIR%\01_build.log" 2>&1
if errorlevel 1 (
    echo         FAILED -- see _logs\01_build.log
    >> "%SUMMARY%" echo [FAIL] compiler build/install -- see 01_build.log
    >> "%SUMMARY%" echo.
    >> "%SUMMARY%" echo Aborted: nothing below was run against a fresh compiler.
    goto :report
)
>> "%SUMMARY%" echo [ OK ] compiler build + install
goto :after_build

:skip_build
echo [1/4] Rebuild skipped by request.
>> "%SUMMARY%" echo [SKIP] compiler build ^(norebuild^)

:after_build

REM --- 2. ctest ---------------------------------------------------------------
echo [2/4] Running ctest ...
ctest --test-dir "%ROOT%\build-clang" --output-on-failure > "%LOGDIR%\02_ctest.log" 2>&1
if errorlevel 1 (
    echo         FAILED -- see _logs\02_ctest.log
    >> "%SUMMARY%" echo [FAIL] ctest -- see 02_ctest.log
) else (
    >> "%SUMMARY%" echo [ OK ] ctest
)
>> "%SUMMARY%" echo.

REM --- 3. Interop contract fixtures -------------------------------------------
echo [3/4] Interop contract fixtures ...
>> "%SUMMARY%" echo --- interop contract fixtures ---

set "IC=tests\interop_contract"

call :run_case "%IC%\c1_03_function_pointer_parameter\c1_03_function_pointer_parameter.nsproj" c1_03_function_pointer_parameter 42 "%FIXLOG%"
call :run_case "%IC%\c1_04_function_pointer_return\c1_04_function_pointer_return.nsproj" c1_04_function_pointer_return 42 "%FIXLOG%"
call :run_case "%IC%\c1_05_raw_pointer_cast\c1_05_raw_pointer_cast.nsproj" c1_05_raw_pointer_cast 42 "%FIXLOG%"
call :run_case "%IC%\c2_11_fixed_field_array\c2_11_fixed_field_array.nsproj" c2_11_fixed_field_array 42 "%FIXLOG%"
call :run_case "%IC%\c2_12_union\c2_12_union.nsproj" c2_12_union 42 "%FIXLOG%"
call :run_case "%IC%\c4_ref_out_in\c4_ref_out_in.nsproj" c4_ref_out_in 42 "%FIXLOG%"
call :run_case "%IC%\c6_struct_math\c6_struct_math.nsproj" c6_struct_math 42 "%FIXLOG%"
call :run_case "%IC%\c7_definite_assignment\c7_definite_assignment.nsproj" c7_definite_assignment 42 "%FIXLOG%"
call :run_case "%IC%\c8_type_root\c8_type_root.nsproj" c8_type_root 42 "%FIXLOG%"

REM Exit 141 = 128 + LlvmTrapReason::CallbackExceptionEscape.
REM A 0 here would mean a NativeSharp exception escaped through the C boundary.
call :run_case "%IC%\c1_14_callback_exception_boundary\c1_14_callback_exception_boundary.nsproj" c1_14_callback_exception_boundary 141 "%FIXLOG%"

REM Links on purpose against a.dll / b.dll, which do not exist. Build+link only.
call :build_only "%IC%\c1_13_two_libraries_same_symbol\c1_13_two_libraries_same_symbol.nsproj" c1_13_two_libraries_same_symbol "%FIXLOG%"

>> "%SUMMARY%" echo.

REM --- 4. Packaged examples ---------------------------------------------------
echo [4/4] Packaged examples ...
>> "%SUMMARY%" echo --- packaged examples ---

call :run_case "examples\project_stage56_c1\Stage56C1InteropCompletion.nsproj" project_stage56_c1 0 "%EXLOG%"
call :run_case "examples\project_stage56_c2c4\Stage56C2C4LanguageCompletion.nsproj" project_stage56_c2c4 0 "%EXLOG%"
call :run_case "examples\project_stage56_c6\Stage56C6StructMath.nsproj" project_stage56_c6 0 "%EXLOG%"
call :run_case "examples\project_stage56_c7\project_stage56_c7.nsproj" project_stage56_c7 0 "%EXLOG%"
call :run_case "examples\project_stage56_c8\project_stage56_c8.nsproj" project_stage56_c8 0 "%EXLOG%"

:report
>> "%SUMMARY%" echo.
>> "%SUMMARY%" echo Passed: %PASSED%
>> "%SUMMARY%" echo Failed: %FAILED%
>> "%SUMMARY%" echo Finished: %DATE% %TIME%

echo.
echo ---------------------------------------------------------------
type "%SUMMARY%"
echo ---------------------------------------------------------------
echo.
echo Logs in: %LOGDIR%

if %FAILED% GTR 0 exit /b 1
exit /b 0

REM ===========================================================================
REM :run_case  <.nsproj path, relative to root>  <label>  <expected exit>  <log>
REM Builds the project at -O0 and -O2, runs it, compares the exit code.
REM ===========================================================================
:run_case
set "CPROJ=%ROOT%\%~1"
set "CNAME=%~2"
set "CEXPECT=%~3"
set "CLOG=%~4"

if not exist "%CPROJ%" (
    call :fail "%CNAME%" "project not found: %CPROJ%"
    exit /b 0
)

for %%O in (O0 O2) do (
    set "NSC_OPT=-%%O"
    set "CEXE=%BINDIR%\%CNAME%_%%O.exe"
    >> "%CLOG%" echo.
    >> "%CLOG%" echo --- %CNAME% -%%O ---
    call "%ROOT%\tools\build_project.cmd" "%CPROJ%" "!CEXE!" >> "%CLOG%" 2>&1
    if errorlevel 1 (
        call :fail "%CNAME% -%%O" "build failed"
    ) else (
        "!CEXE!" >> "%CLOG%" 2>&1
        set /a RC=!ERRORLEVEL!
        >> "%CLOG%" echo [exit !RC!]
        if "!RC!"=="%CEXPECT%" (
            call :ok "%CNAME% -%%O" "exit !RC!"
        ) else (
            call :fail "%CNAME% -%%O" "exit !RC!, expected %CEXPECT%"
        )
    )
)
exit /b 0

REM ===========================================================================
REM :build_only  <.nsproj path, relative to root>  <label>  <log>
REM Compiles and links but never executes.
REM ===========================================================================
:build_only
set "CPROJ=%ROOT%\%~1"
set "CNAME=%~2"
set "CLOG=%~3"

if not exist "%CPROJ%" (
    call :fail "%CNAME%" "project not found: %CPROJ%"
    exit /b 0
)

for %%O in (O0 O2) do (
    set "NSC_OPT=-%%O"
    set "CEXE=%BINDIR%\%CNAME%_%%O.exe"
    >> "%CLOG%" echo.
    >> "%CLOG%" echo --- %CNAME% -%%O ^(build+link only^) ---
    call "%ROOT%\tools\build_project.cmd" "%CPROJ%" "!CEXE!" >> "%CLOG%" 2>&1
    if errorlevel 1 (
        call :fail "%CNAME% -%%O" "build/link failed"
    ) else (
        call :ok "%CNAME% -%%O" "built and linked, not executed"
    )
)
exit /b 0

:ok
>> "%SUMMARY%" echo [ OK ] %~1 -- %~2
set /a PASSED+=1
exit /b 0

:fail
>> "%SUMMARY%" echo [FAIL] %~1 -- %~2
set /a FAILED+=1
exit /b 0
