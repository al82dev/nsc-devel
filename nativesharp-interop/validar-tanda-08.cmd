@echo off
setlocal
set "PATH=A:\al82dev-clang\bin;A:\nsc-interop-build\tanda1-ON\bin;%PATH%"
set "TMP=A:\nsc-interop-build\temp"
set "TEMP=%TMP%"
cmake -S "%~dp0.." -B A:/nsc-interop-build/tanda1-ON -DNSC_BUILD_TESTS=ON
if errorlevel 1 exit /b 1
call "%~dp0construir-frontend-tanda1.cmd" "nsc nsc_cpp_session_tests nsc_precompiled_syntax_node_index_tests nsc_syntax_tree_serialization_equivalence_tests nsc_project_oop_lowering_tests nsc_namespace_function_llvm_tests" ON
if errorlevel 1 exit /b 1
call "%~dp0construir-frontend-tanda1.cmd" nsc_frontend OFF
if errorlevel 1 exit /b 1
set "TASK_FAILED=0"
echo === Pruebas de sesion Clang ===
A:\nsc-interop-build\tanda1-ON\bin\nsc_cpp_session_tests.exe A:\al82dev-clang\lib\clang\23 A:\nsc-interop-build A:\al82dev-clang\bin\clang.exe
if errorlevel 1 set "TASK_FAILED=1"
echo === Restauracion de sintaxis y equivalencia de serializacion ===
A:\nsc-interop-build\tanda1-ON\tests\nsc_precompiled_syntax_node_index_tests.exe
if errorlevel 1 set "TASK_FAILED=1"
A:\nsc-interop-build\tanda1-ON\tests\nsc_syntax_tree_serialization_equivalence_tests.exe
if errorlevel 1 set "TASK_FAILED=1"
echo === Restauracion de MIR ===
set "NSC_PROJECT_OOP_LOWERING_FOCUS=mir-cache-restore-depth"
A:\nsc-interop-build\tanda1-ON\tests\nsc_project_oop_lowering_tests.exe
if errorlevel 1 set "TASK_FAILED=1"
set "NSC_PROJECT_OOP_LOWERING_FOCUS="
echo === Identidad de codigo MIR ===
pushd "%TEMP%"
A:\nsc-interop-build\tanda1-ON\tests\nsc_namespace_function_llvm_tests.exe
if errorlevel 1 set "TASK_FAILED=1"
popd
for %%T in (valores incompleto record-c union layout callback enum-funcion) do call :run_case %%T
for %%T in (global-puntero-const flotante-c inline-c prototipos-c typedef-c bitfields-c tls-c) do call :run_case %%T
for %%T in (
  pruebas-tanda-08-incompleto\validar-opaco.cmd
  pruebas-tanda-08-record-c\precompiled-bool\validar.cmd
  pruebas-tanda-08-record-c\precompiled-bool\validar-cache.cmd
  pruebas-tanda-08-record-c\dll-bool\validar.cmd
  pruebas-tanda-08-atomic-global\validar.cmd
  pruebas-tanda-08-atomic-global\validar-hilos.cmd
  pruebas-tanda-08-atomic-global\validar-precompilado.cmd
  pruebas-tanda-08-atomic-global-dll\validar.cmd
  pruebas-tanda-08-atomic-global-matrix\validar.cmd
  pruebas-tanda-08-atomic-global-matrix\validar-extra.cmd
  pruebas-tanda-08-atomic-global-matrix\validar-calificados.cmd
  pruebas-tanda-08-atomic-global-matrix\validar-mas-punteros.cmd
  pruebas-tanda-08-atomic-global-matrix\validar-punteros-tls.cmd
  pruebas-tanda-08-atomic-global-matrix\validar-calificados-tls.cmd
  pruebas-tanda-08-atomic-global-matrix\validar-long-tls.cmd
  pruebas-tanda-08-atomic-global-matrix\validar-mas-calificados.cmd
  pruebas-tanda-08-atomic-global-matrix\validar-widths-global.cmd
  pruebas-tanda-08-atomic-global-matrix\validar-widths-tls.cmd
  pruebas-tanda-08-atomic-global-matrix\validar-widths-hilos.cmd
  pruebas-tanda-08-global-static\validar.cmd
  pruebas-tanda-08-global-static-matriz\validar.cmd
  pruebas-tanda-08-callback\validar-record-precompilado.cmd
  pruebas-tanda-08-enum-funcion\validar-callback-precompilado.cmd
  pruebas-tanda-08-typedef-c\validar-puntero.cmd
  pruebas-tanda-08-typedef-c\validar-enumerador.cmd
  pruebas-tanda-08-typedef-c\validar-enum-mixto.cmd
  pruebas-tanda-08-typedef-c\validar-enum-entero.cmd
  pruebas-tanda-08-typedef-c\validar-enum-aritmetica.cmd
  pruebas-tanda-08-typedef-c\validar-agregados.cmd
  pruebas-tanda-08-bitfields-c\validar-volatile.cmd
  pruebas-tanda-08-bitfields-c\validar-volatile-metadata.cmd
  pruebas-tanda-08-bitfields-c\validar-union.cmd
  pruebas-tanda-08-bitfields-c\validar-metadata.cmd
  pruebas-tanda-08-bitfields-c\validar-matriz-volatile.cmd
  pruebas-tanda-08-tls-c\validar-dll.cmd
  pruebas-tanda-08-tls-c\directo\validar-nsc-directo.cmd
  pruebas-tanda-08-tls-c\directo\validar-nsc-otro-hilo.cmd
  pruebas-tanda-08-tls-c\directo\validar-nsc-escalares-con-import.cmd
  pruebas-tanda-08-tls-c\directo\validar-nsc-escalares-con-entry-e-import.cmd
  pruebas-tanda-08-tls-c\directo\validar-escalar-crt.cmd
  pruebas-tanda-08-tls-c\directo\validar-relleno-cero.cmd
  pruebas-tanda-08-tls-c\directo\validar-escalar-carga-dinamica-con-import.cmd
  pruebas-tanda-08-tls-c\directo\validar-guardia-dll.cmd
  pruebas-tanda-08-tls-c\static-global\validar.cmd
  pruebas-tanda-08-tls-c\static-global\validar-c11.cmd
  pruebas-tanda-08-tls-c\atomic\validar.cmd
  pruebas-tanda-08-tls-c\atomic\precompiled\validar.cmd
  pruebas-tanda-08-tls-c\precompiled\validar.cmd
  pruebas-tanda-08-tls-c\precompiled\validar-tls-transitivo.cmd
  pruebas-tanda-08-tls-c\precompiled\validar-dll-precompilado.cmd
  pruebas-tanda-08-tls-c\pointer\validar.cmd
  pruebas-tanda-08-tls-c\array\validar.cmd
  pruebas-tanda-08-tls-c\scalars\validar.cmd
  pruebas-tanda-08-tls-c\lld\validar-seleccion.cmd
  pruebas-tanda-08-tls-c\lld\validar-sin-entorno.cmd
  pruebas-tanda-08-tls-c\lld\validar-cambio-enlazador.cmd
  pruebas-tanda-08-tls-c\lld\validar-auto-tls.cmd
  pruebas-tanda-08-tls-c\lld\validar-auto-bitcode.cmd
  pruebas-tanda-08-tls-c\lld\validar-sidecar-nuevo.cmd
  pruebas-tanda-08-tls-c\lld\validar-emision.cmd
  pruebas-tanda-08-tls-c\lld\validar-emision-auto.cmd
  pruebas-tanda-08-tls-c\lld\validar-dll-emision-auto.cmd
  pruebas-tanda-08-tls-c\lld\link-lib\validar.cmd
  pruebas-tanda-08-tls-c\lld\validar-build.cmd
  pruebas-tanda-08-tls-c\lld\validar-transitivo.cmd
  pruebas-tanda-08-meta-c\validar-productor.cmd
  pruebas-tanda-08-meta-c\validar-sombras.cmd
) do call :run_script %%T
echo === Publicacion y consumo de tipo C por NSMETA ===
call "%~dp0pruebas-tanda-08-meta-c\validar.cmd"
if errorlevel 1 set "TASK_FAILED=1"
if "%TASK_FAILED%"=="1" (
  echo La validacion de la tanda 08 contiene fallos; revisar la salida anterior.
  exit /b 1
)
echo Casos agrupados de la tanda 08 terminados. Revisar alcance y evidencia antes de cerrar items.
exit /b 0

:run_case
echo === Caso %~1 ===
call "%~dp0pruebas-tanda-08-%~1\validar.cmd"
if errorlevel 1 (
  echo FALLO: caso %~1
  set "TASK_FAILED=1"
)
exit /b 0

:run_script
echo === Caso %~1 ===
if not exist "%~dp0%~1" (
  echo Falta el validador %~1.
  set "TASK_FAILED=1"
  exit /b 0
)
call "%~dp0%~1"
if errorlevel 1 (
  echo FALLO: %~1
  set "TASK_FAILED=1"
)
exit /b 0
