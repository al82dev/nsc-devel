@echo off
setlocal EnableExtensions EnableDelayedExpansion

REM ---------------------------------------------------------------------------
REM NativeSharp -- real Windows execution checklist.
REM
REM Builds and installs the compiler, runs ctest, then BUILDS AND RUNS every
REM interop contract fixture and every Stage 56 packaged example at -O0 and -O2,
REM comparing the real process exit code against the documented contract.
REM
REM MODOS
REM   run_windows_checklist.bat             completo: configure + build + install
REM                                         + ctest + fixtures + ejemplos
REM   run_windows_checklist.bat quick       build incremental + install
REM                                         + fixtures + ejemplos, SIN ctest
REM   run_windows_checklist.bat build       build incremental + install, nada mas
REM   run_windows_checklist.bat norebuild   sin compilar: ctest + fixtures + ejemplos
REM
REM El build SIEMPRE fue incremental -- Ninja solo recompila lo que cambio. Lo que
REM cuesta es ctest (~4.5 min de los ~6). `quick` es el modo del ciclo de trabajo:
REM se salta ctest y ademas el `cmake -S . -B` de reconfiguracion, que en una
REM iteracion donde solo cambiaron .cpp no aporta nada.
REM
REM Corre `run_windows_checklist.bat` completo antes de dar una ronda por buena:
REM ctest es lo unico que demuestra que no se rompio nada.
REM
REM Todo queda en _logs\. Leer _logs\SUMMARY.txt primero.
REM
REM NOTA: mantener finales de linea CRLF. cmd.exe resuelve "call :label" por
REM offset de bytes asumiendo CRLF; con LF se desalinea y salta llamadas enteras.
REM ---------------------------------------------------------------------------

set "ROOT=%~dp0"
if "%ROOT:~-1%"=="\" set "ROOT=%ROOT:~0,-1%"
set "LOGDIR=%ROOT%\_logs"
set "BINDIR=%LOGDIR%\bin"
set "SUMMARY=%LOGDIR%\SUMMARY.txt"
set "FIXLOG=%LOGDIR%\03_fixtures.log"
set "EXLOG=%LOGDIR%\04_examples.log"
set "BUILDDIR=%ROOT%\build-clang"
set "PREFIX=C:\native-sharp"

set "MODE=%~1"
if "%MODE%"=="" set "MODE=full"

if not exist "%LOGDIR%" mkdir "%LOGDIR%"
if not exist "%BINDIR%" mkdir "%BINDIR%"

set /a PASSED=0
set /a FAILED=0

> "%SUMMARY%" echo NativeSharp -- Windows execution checklist
>> "%SUMMARY%" echo Started: %DATE% %TIME%
>> "%SUMMARY%" echo Root   : %ROOT%
>> "%SUMMARY%" echo Modo   : %MODE%
>> "%SUMMARY%" echo.

> "%FIXLOG%" echo === interop contract fixtures ===
> "%EXLOG%" echo === packaged examples ===

REM --- 1. Build + install -----------------------------------------------------
if /I "%MODE%"=="norebuild" goto :skip_build
if /I "%MODE%"=="full" goto :full_build

echo [1/5] Build incremental e instalacion en %PREFIX% ...
cmake --build "%BUILDDIR%" --parallel > "%LOGDIR%\01_build.log" 2>&1
if errorlevel 1 goto :build_failed
cmake --install "%BUILDDIR%" --prefix "%PREFIX%" >> "%LOGDIR%\01_build.log" 2>&1
if errorlevel 1 goto :build_failed
>> "%SUMMARY%" echo [ OK ] build incremental + install
goto :after_build

:full_build
REM Se corren los tres pasos aqui en vez de delegar en build-all.bat, y la razon es un bug real
REM que costo una corrida entera: build-all.bat termina con `cmake --install`, y el instalador
REM devuelve 0 aunque `cmake --build` haya fallado antes -- instala lo que ya existia. El script
REM leia ese 0, seguia adelante, y corria toda la matriz contra el nsc.exe VIEJO. Un fallo de
REM compilacion se presentaba entonces como fixtures rojos con errores absurdos ("unexpected
REM character in source" para sintaxis que el compilador nuevo si soporta), que es la peor forma
REM posible de reportarlo. Cada paso se comprueba por separado.
echo [1/5] Reconstruccion completa e instalacion en %PREFIX% ...
cmake -S "%ROOT%" -B "%BUILDDIR%" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER="C:/clang-llvm/bin/clang-cl.exe" -DCMAKE_CXX_COMPILER="C:/clang-llvm/bin/clang-cl.exe" -DCMAKE_LINKER="C:/clang-llvm/bin/lld-link.exe" -DCMAKE_EXE_LINKER_FLAGS="-fuse-ld=lld" -DCMAKE_DISABLE_FIND_PACKAGE_LLVM=TRUE -DNSC_LLVM_MONOLITHIC_LIBRARY="C:/clang-llvm/lib/LLVM-C.lib" -DNSC_BUILD_TESTS=ON -DNSC_WARNINGS_AS_ERRORS=ON > "%LOGDIR%\01_build.log" 2>&1
if errorlevel 1 goto :build_failed
cmake --build "%BUILDDIR%" --parallel >> "%LOGDIR%\01_build.log" 2>&1
if errorlevel 1 goto :build_failed
cmake --install "%BUILDDIR%" --prefix "%PREFIX%" >> "%LOGDIR%\01_build.log" 2>&1
if errorlevel 1 goto :build_failed
>> "%SUMMARY%" echo [ OK ] compiler build + install
goto :after_build

:build_failed
echo         FALLO -- ver _logs\01_build.log
>> "%SUMMARY%" echo [FAIL] compiler build/install -- ver 01_build.log
>> "%SUMMARY%" echo.
>> "%SUMMARY%" echo Abortado: nada de lo de abajo corrio contra un compilador fresco.
goto :report

:skip_build
echo [1/5] Build omitido.
>> "%SUMMARY%" echo [SKIP] compiler build ^(norebuild^)

:after_build
if /I "%MODE%"=="build" goto :report

REM --- 2. ctest ---------------------------------------------------------------
if /I "%MODE%"=="quick" goto :skip_ctest
echo [2/5] Corriendo ctest ...
ctest --test-dir "%BUILDDIR%" --output-on-failure > "%LOGDIR%\02_ctest.log" 2>&1
if errorlevel 1 (
    echo         FALLO -- ver _logs\02_ctest.log
    >> "%SUMMARY%" echo [FAIL] ctest -- ver 02_ctest.log
) else (
    >> "%SUMMARY%" echo [ OK ] ctest
)
goto :after_ctest

:skip_ctest
echo [2/5] ctest omitido ^(modo quick^).
>> "%SUMMARY%" echo [SKIP] ctest ^(modo quick -- correr el modo completo antes de cerrar la ronda^)

:after_ctest
>> "%SUMMARY%" echo.

REM --- 3. Interop contract fixtures -------------------------------------------
echo [3/5] Fixtures de contrato de interop ...
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
REM Un 0 aqui significaria que una excepcion NativeSharp se escapo por el borde C.
call :run_case "%IC%\c1_14_callback_exception_boundary\c1_14_callback_exception_boundary.nsproj" c1_14_callback_exception_boundary 141 "%FIXLOG%"

REM Linkea a proposito contra a.dll / b.dll, que no existen. Solo build+link.
call :build_only "%IC%\c1_13_two_libraries_same_symbol\c1_13_two_libraries_same_symbol.nsproj" c1_13_two_libraries_same_symbol "%FIXLOG%"

>> "%SUMMARY%" echo.
>> "%SUMMARY%" echo --- language contract fixtures ---

set "LC=tests\language"
call :run_case "%LC%\l1_conditional\l1_conditional.nsproj" l1_conditional 42 "%FIXLOG%"
call :run_case "%LC%\l2_dowhile\l2_dowhile.nsproj" l2_dowhile 42 "%FIXLOG%"
call :run_case "%LC%\l3_switch\l3_switch.nsproj" l3_switch 42 "%FIXLOG%"

>> "%SUMMARY%" echo.

REM --- 4. Packaged examples ---------------------------------------------------
echo [4/5] Ejemplos empaquetados ...
>> "%SUMMARY%" echo --- packaged examples ---

call :run_case "examples\project_stage56_c1\Stage56C1InteropCompletion.nsproj" project_stage56_c1 0 "%EXLOG%"
call :run_case "examples\project_stage56_c2c4\Stage56C2C4LanguageCompletion.nsproj" project_stage56_c2c4 0 "%EXLOG%"
call :run_case "examples\project_stage56_c6\Stage56C6StructMath.nsproj" project_stage56_c6 0 "%EXLOG%"
call :run_case "examples\project_stage56_c7\project_stage56_c7.nsproj" project_stage56_c7 0 "%EXLOG%"
call :run_case "examples\project_stage56_c8\project_stage56_c8.nsproj" project_stage56_c8 0 "%EXLOG%"

REM --- 5. Regresion de bugs reportados ----------------------------------------
REM Solo en el modo completo. Los ocho casos de tests\bug_triage tambien estan bajo ctest
REM (nsc.stage56.bugfix), pero ahi solo se verifica que COMPILAN: en Windows no hay Wine, asi
REM que la mitad de ejecucion la corre este script. Dos de esos bugs eran silenciosos -- el
REM programa compilaba y devolvia el valor equivocado -- y para esos, ejecutar es la unica
REM verificacion que existe.
if /I "%MODE%"=="quick" goto :report
if /I "%MODE%"=="build" goto :report
>> "%SUMMARY%" echo.
>> "%SUMMARY%" echo --- regresion de bugs reportados ---
echo [5/5] Regresion de bugs reportados ...
call "%ROOT%\run_bug_triage.bat" > "%LOGDIR%\05_triage_run.log" 2>&1
if errorlevel 1 (
    call :fail "bug_triage" "hay bugs vivos -- ver _logs\TRIAGE.txt"
) else (
    call :ok "bug_triage" "16/16, cero vivos"
)

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
echo Logs en: %LOGDIR%

if %FAILED% GTR 0 exit /b 1
exit /b 0

REM ===========================================================================
REM :run_case  <ruta .nsproj relativa a la raiz>  <etiqueta>  <exit esperado>  <log>
REM ===========================================================================
:run_case
set "CPROJ=%ROOT%\%~1"
set "CNAME=%~2"
set "CEXPECT=%~3"
set "CLOG=%~4"

if not exist "%CPROJ%" (
    call :fail "%CNAME%" "proyecto no encontrado"
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
            call :fail "%CNAME% -%%O" "exit !RC!, se esperaba %CEXPECT%"
        )
    )
)
exit /b 0

REM ===========================================================================
REM :build_only  <ruta .nsproj relativa a la raiz>  <etiqueta>  <log>
REM ===========================================================================
:build_only
set "CPROJ=%ROOT%\%~1"
set "CNAME=%~2"
set "CLOG=%~3"

if not exist "%CPROJ%" (
    call :fail "%CNAME%" "proyecto no encontrado"
    exit /b 0
)

for %%O in (O0 O2) do (
    set "NSC_OPT=-%%O"
    set "CEXE=%BINDIR%\%CNAME%_%%O.exe"
    >> "%CLOG%" echo.
    >> "%CLOG%" echo --- %CNAME% -%%O build+link ---
    call "%ROOT%\tools\build_project.cmd" "%CPROJ%" "!CEXE!" >> "%CLOG%" 2>&1
    if errorlevel 1 (
        call :fail "%CNAME% -%%O" "build/link failed"
    ) else (
        call :ok "%CNAME% -%%O" "compila y linkea, no se ejecuta"
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
