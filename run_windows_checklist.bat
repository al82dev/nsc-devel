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
REM LIMPIEZA: cada corrida BORRA _logs\ entero antes de empezar, en todos los modos, para
REM que no pueda leerse el resumen de una corrida anterior creyendo que es el de esta.
REM Tambien borra build-msvc\, que es un arbol de build muerto. build-clang\ NO se toca.
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
set "STALEDIR=%ROOT%\build-msvc"
set "PREFIX=C:\native-sharp"
set "LLVMROOT=C:\clang-llvm"
set "LLVM=%LLVMROOT:\=/%"

set "MODE=%~1"
if "%MODE%"=="" set "MODE=full"

REM --- 0. Limpieza previa ------------------------------------------------------
REM _logs SE BORRA ENTERO en cada corrida, a proposito. Un log viejo que sobrevive es peor
REM que no tener log: ya paso una vez que se leyo un SUMMARY.txt de una corrida anterior
REM creyendo que era el de la actual. Si el script muere a la mitad, lo que quede en _logs
REM es de ESTA corrida y de ninguna otra.
if exist "%LOGDIR%" rmdir /s /q "%LOGDIR%" 2>nul

REM STALEDIR es un arbol de build MUERTO: el unico que se usa es build-clang (ver BUILDDIR
REM arriba, y build-all.bat). build-msvc no lo referencia nada y sus targets se quedaron en
REM Stage 52. Se borra para que nadie lo confunda con el build vivo ni lo revive por accidente.
REM OJO: aqui NO se toca %BUILDDIR%. El build es y sigue siendo incremental -- Ninja recompila
REM solo lo que cambio, y borrarlo convertiria cada corrida en una reconstruccion completa.
if exist "%STALEDIR%" rmdir /s /q "%STALEDIR%" 2>nul

REM --- Estado de build-clang\ --------------------------------------------------------
REM Distinto de STALEDIR: aqui build-clang SI es el arbol vivo, pero puede no servir de dos
REM formas, y las dos se borran.
REM
REM   AJENO. El que viajo desde D:\nsc-devel lleva "D:/nsc-devel" escrito dentro de
REM   CMakeCache.txt, build.ninja, CTestTestfile.cmake y de cada .obj; CMake se niega a
REM   reutilizarlo desde otro sitio y Ninja tampoco puede regenerarlo. Reemplazar la ruta a
REM   mano dejaria un arbol de build en el que nadie puede confiar, que en este proyecto es
REM   lo peor que puede pasar: la suite es la unica red que hay.
REM
REM   A MEDIAS. Una configuracion que fallo tambien deja un CMakeCache.txt, con sus valores
REM   fallidos dentro. `CMAKE_CXX_COMPILER_WORKS:INTERNAL=1` solo aparece cuando la
REM   configuracion llego al final, asi que su ausencia es la senal.
REM
REM En los dos casos se fuerza MODE=full. Sin eso, `quick`, `build` o `norebuild` se saltan
REM la reconfiguracion y correrian la matriz entera contra un arbol que acaba de
REM desaparecer -- la misma clase de fallo que documenta el comentario de :full_build.
if not exist "%BUILDDIR%\CMakeCache.txt" goto :cache_ok
set "CACHEROOT="
for /f "tokens=2 delims==" %%A in ('findstr /B /C:"CMAKE_HOME_DIRECTORY:INTERNAL=" "%BUILDDIR%\CMakeCache.txt" 2^>nul') do set "CACHEROOT=%%A"
set "ROOTFWD=%ROOT:\=/%"
if /I not "%CACHEROOT%"=="%ROOTFWD%" goto :cache_alien
findstr /B /C:"CMAKE_CXX_COMPILER_WORKS:INTERNAL=1" "%BUILDDIR%\CMakeCache.txt" >nul 2>&1
if errorlevel 1 goto :cache_partial
goto :cache_ok

:cache_alien
echo   build-clang\ se genero desde "%CACHEROOT%" y este arbol esta en "%ROOTFWD%".
echo   Se borra y se reconstruye completo. Modo forzado a `full`.
rmdir /s /q "%BUILDDIR%" 2>nul
set "MODE=full"
goto :cache_ok

:cache_partial
echo   build-clang\ quedo de una configuracion que no termino. Se borra.
echo   Modo forzado a `full`.
rmdir /s /q "%BUILDDIR%" 2>nul
set "MODE=full"

:cache_ok

if not exist "%LOGDIR%" mkdir "%LOGDIR%"
if not exist "%BINDIR%" mkdir "%BINDIR%"

set /a PASSED=0
set /a FAILED=0

> "%SUMMARY%" echo NativeSharp -- Windows execution checklist
>> "%SUMMARY%" echo Started: %DATE% %TIME%
>> "%SUMMARY%" echo Root   : %ROOT%
>> "%SUMMARY%" echo Modo   : %MODE%
>> "%SUMMARY%" echo Limpieza: _logs borrado al arrancar; build-clang intacto (incremental)
>> "%SUMMARY%" echo.

> "%FIXLOG%" echo === interop contract fixtures ===
> "%EXLOG%" echo === packaged examples ===

REM --- 0b. Preflight del toolchain --------------------------------------------
REM MEDIDO en la primera corrida en A:. El CMakeCache viejo (el de D:) tenia
REM   CMAKE_MT:FILEPATH=C:/clang-llvm/bin/llvm-mt.exe
REM porque en aquella maquina C:\clang-llvm\bin estaba en el PATH y CMake lo encontro solo.
REM Aqui no lo esta: CMake dejo CMAKE_MT-NOTFOUND, el enlace del programa de prueba fallo con
REM "no such file or directory", y la configuracion murio en `project()` con un mensaje que
REM habla del compilador y no de la herramienta que falta. Tres segundos de corrida y un log
REM que apunta al sitio equivocado.
REM
REM Por eso las rutas del toolchain se pasan TODAS explicitas mas abajo, y por eso se
REM comprueban aqui: si falta una, el script lo dice por su nombre en vez de dejar que CMake
REM lo descubra a su manera.
if not exist "%LLVMROOT%\bin\clang-cl.exe" goto :no_toolchain
if not exist "%LLVMROOT%\bin\llvm-mt.exe" goto :no_toolchain
if not exist "%LLVMROOT%\bin\lld-link.exe" goto :no_toolchain
if not exist "%LLVMROOT%\lib\LLVM-C.lib" goto :no_toolchain
goto :toolchain_ok

:no_toolchain
echo         FALTA EL TOOLCHAIN en %LLVMROOT%
>> "%SUMMARY%" echo [FAIL] toolchain -- falta algo en %LLVMROOT% ^(clang-cl, llvm-mt, lld-link, LLVM-C.lib^)
>> "%SUMMARY%" echo.
>> "%SUMMARY%" echo Abortado antes de compilar: no hay con que.
goto :report

:toolchain_ok

REM --- 1. Build + install -----------------------------------------------------
if /I "%MODE%"=="norebuild" goto :skip_build
if /I "%MODE%"=="full" goto :full_build

echo [1/5] Build incremental e instalacion en %PREFIX% ...
cmake --build "%BUILDDIR%" --parallel -- -k 0 > "%LOGDIR%\01_build.log" 2>&1
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
cmake -S "%ROOT%" -B "%BUILDDIR%" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER="%LLVM%/bin/clang-cl.exe" -DCMAKE_LINKER="%LLVM%/bin/lld-link.exe" -DCMAKE_MT="%LLVM%/bin/llvm-mt.exe" -DCMAKE_RC_COMPILER="%LLVM%/bin/llvm-rc.exe" -DCMAKE_EXE_LINKER_FLAGS="-fuse-ld=lld" -DCMAKE_DISABLE_FIND_PACKAGE_LLVM=TRUE -DNSC_LLVM_MONOLITHIC_LIBRARY="%LLVM%/lib/LLVM-C.lib" -DNSC_BUILD_TESTS=ON -DNSC_WARNINGS_AS_ERRORS=ON > "%LOGDIR%\01_build.log" 2>&1
if errorlevel 1 goto :build_failed
cmake --build "%BUILDDIR%" --parallel -- -k 0 >> "%LOGDIR%\01_build.log" 2>&1
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

REM --- 1b. Sincronizar las bibliotecas de proyecto -----------------------------
REM VA AQUI, ANTES DE CTEST, Y ESE ES EL ARREGLO.
REM
REM `lib\NativeSharp.Core` y `lib\NativeSharp.Core.Collections.Generic` NO son fuentes: son
REM copias que tools\prepare_libraries.cmd borra y vuelve a traer de %NS_LIB_ROOT% en cada
REM build de proyecto. Lo dice lib\.source.txt, que ese mismo script escribe.
REM
REM Hasta ahora el unico paso que las sincronizaba era el 3 (fixtures), pero 27 de los tests
REM de ctest -- el paso 2 -- las necesitan. Es decir: ctest dependia de un directorio que un
REM paso POSTERIOR era el responsable de crear. Mientras lib\ sobrevivia de una corrida
REM anterior nadie lo notaba; en cuanto quedo ausente, ctest cayo con 27 rojos cuyo mensaje
REM ("referenced project does not exist") no menciona bibliotecas por ningun lado, y el paso 3
REM los dejo verdes acto seguido. Medido el 2026-09-02; ver 83 seccion 4.
REM
REM Ademas es la sincronizacion CORRECTA: se hace justo despues del install, asi que lib\
REM corresponde al compilador que se acaba de instalar y no al de la corrida anterior.
echo [1b/5] Sincronizando bibliotecas de proyecto desde el prefijo instalado ...
call "%ROOT%\tools\prepare_libraries.cmd" >> "%LOGDIR%\01_build.log" 2>&1
if errorlevel 1 goto :lib_failed
>> "%SUMMARY%" echo [ OK ] lib/ sincronizado desde el prefijo instalado
goto :lib_ready

:lib_failed
echo         FALLO -- no se pudieron sincronizar las bibliotecas. Ver _logs\01_build.log
>> "%SUMMARY%" echo [FAIL] lib/ -- prepare_libraries.cmd fallo, ver 01_build.log
>> "%SUMMARY%" echo.
>> "%SUMMARY%" echo Abortado: sin lib/ sincronizado, ctest y las fixtures miden otra cosa.
goto :report

:lib_ready

REM --- 2. ctest ---------------------------------------------------------------
if /I "%MODE%"=="quick" goto :skip_ctest
echo [2/5] Corriendo ctest ...
ctest --test-dir "%BUILDDIR%" --output-on-failure > "%LOGDIR%\02_ctest.log" 2>&1
REM Un ctest rojo TIENE que contar en PASSED/FAILED. Antes no contaba: escribia [FAIL] en el
REM resumen y no tocaba los contadores, asi que la corrida del 2026-09-02 termino diciendo
REM "Passed: 71 / Failed: 0" con 27 de 242 tests en rojo, y salio con codigo 0. El paso mas
REM importante de los cinco era el unico que no entraba en el veredicto.
REM
REM Cuenta como UNA unidad y son 242 tests; el detalle esta en 02_ctest.log. Lo que importa es
REM que un solo comando no pueda decir que todo salio bien cuando no salio bien.
if errorlevel 1 goto :ctest_failed
>> "%SUMMARY%" echo [ OK ] ctest ^(242 tests^)
set /a PASSED+=1
goto :after_ctest

:ctest_failed
echo         FALLO -- ver _logs\02_ctest.log
>> "%SUMMARY%" echo [FAIL] ctest ^(242 tests^) -- ver 02_ctest.log
set /a FAILED+=1
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
>> "%SUMMARY%" echo --- milestone CC1 fixtures ---

set "CC=tests\cc1"
call :run_case "%CC%\c1_constants\c1_constants.nsproj" cc1_c1_constants 42 "%FIXLOG%"
call :run_case "%CC%\c2_ternary_ownership\c2_ternary_ownership.nsproj" cc1_c2_ternary_ownership 42 "%FIXLOG%"
call :run_case "%CC%\c3_dowhile_flow\c3_dowhile_flow.nsproj" cc1_c3_dowhile_flow 42 "%FIXLOG%"
call :run_case "%CC%\c4_catch_continue\c4_catch_continue.nsproj" cc1_c4_catch_continue 42 "%FIXLOG%"
call :run_case "%CC%\c5_catch_break\c5_catch_break.nsproj" cc1_c5_catch_break 42 "%FIXLOG%"
call :run_case "%CC%\c6_catch_return\c6_catch_return.nsproj" cc1_c6_catch_return 42 "%FIXLOG%"
call :run_case "%CC%\c7_finally_switch\c7_finally_switch.nsproj" cc1_c7_finally_switch 42 "%FIXLOG%"
call :run_case "%CC%\c8_loop_catch_switch\c8_loop_catch_switch.nsproj" cc1_c8_loop_catch_switch 42 "%FIXLOG%"
call :run_case "%CC%\c9_sizeof\c9_sizeof.nsproj" cc1_c9_sizeof 42 "%FIXLOG%"
call :run_case "%CC%\c10_stackalloc\c10_stackalloc.nsproj" cc1_c10_stackalloc 42 "%FIXLOG%"
call :run_case "%CC%\c11_raw_new\c11_raw_new.nsproj" cc1_c11_raw_new 42 "%FIXLOG%"
call :run_case "%CC%\c12_pointer_arith\c12_pointer_arith.nsproj" cc1_c12_pointer_arith 42 "%FIXLOG%"
call :run_case "%CC%\c13_increment\c13_increment.nsproj" cc1_c13_increment 42 "%FIXLOG%"
call :run_case "%CC%\c14_compound_assign\c14_compound_assign.nsproj" cc1_c14_compound_assign 42 "%FIXLOG%"
call :run_case "%CC%\c15_arrow_member\c15_arrow_member.nsproj" cc1_c15_arrow_member 42 "%FIXLOG%"
call :run_case "%CC%\c16_enum_layout\c16_enum_layout.nsproj" cc1_c16_enum_layout 42 "%FIXLOG%"

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
