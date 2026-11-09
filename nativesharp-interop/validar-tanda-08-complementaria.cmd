@echo off
rem EN: Complementary tanda 08 gate. Runs the suites added while closing T08-002 to T08-020 and the T08-D
rem     decisions that validar-tanda-08.cmd does not run, and the derived runs of validar-tanda-08-derivados.py.
rem     Helper scripts that need arguments are run by their own .cmd validators. Use it after validar-tanda-08.cmd.
rem ES: Gate complementario de la tanda 08. Ejecuta las suites agregadas al cerrar T08-002 a T08-020 y las
rem     decisiones T08-D que validar-tanda-08.cmd no ejecuta, y las ejecuciones derivadas de
rem     validar-tanda-08-derivados.py. Los auxiliares que necesitan argumentos los ejecutan sus propios .cmd. Se usa
rem     despues de validar-tanda-08.cmd.
setlocal
set "PATH=A:\al82dev-clang\bin;A:\nsc-interop-build\tanda1-ON\bin;%PATH%"
set "TMP=A:\nsc-interop-build\temp"
set "TEMP=%TMP%"
set "NSC_LINKER=A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
set "TASK_FAILED=0"
pushd "%~dp0.."
for %%T in (
  pruebas-tanda-08-arreglos-c\validar-arreglos.py
  pruebas-tanda-08-arreglos-c\validar-arreglos-punteros-campo.py
  pruebas-tanda-08-calificadores-c\validar-calificadores.py
  pruebas-tanda-08-callbacks-c\validar-callbacks.py
  pruebas-tanda-08-callbacks-c\validar-identidad-funcion-c.py
  pruebas-tanda-08-callbacks-c\validar-vectorcall.py
  pruebas-tanda-08-colisiones-c\validar-colisiones.py
  pruebas-tanda-08-colisiones-c\validar-dependencias-privadas.py
  pruebas-tanda-08-enteros-c\validar-anchos.py
  pruebas-tanda-08-enum-c\validar-identidad.py
  pruebas-tanda-08-enum-c\validar-valores.py
  pruebas-tanda-08-flotante-c\validar-formatos.py
  pruebas-tanda-08-funciones-c\validar-declaraciones.py
  pruebas-tanda-08-funciones-c\validar-static-encabezado.py
  pruebas-tanda-08-funciones-c\validar-visibilidad-enlace.py
  pruebas-tanda-08-globales-c\validar-arreglos.py
  pruebas-tanda-08-globales-c\validar-atomicas.py
  pruebas-tanda-08-globales-c\validar-atomicas-record.py
  pruebas-tanda-08-globales-c\validar-consumo.py
  pruebas-tanda-08-globales-c\validar-direccion-const.py
  pruebas-tanda-08-globales-c\validar-enum-record.py
  pruebas-tanda-08-globales-c\validar-escalares-calificados.py
  pruebas-tanda-08-globales-c\validar-tls.py
  pruebas-tanda-08-identidad-c\cache-c-directa\validar.py
  pruebas-tanda-08-identidad-c\cache-runtime-transitivo\validar.py
  pruebas-tanda-08-identidad-c\funcion-direccion-c\validar.py
  pruebas-tanda-08-identidad-c\global-externa-variantes\validar-arreglo-incompleto.py
  pruebas-tanda-08-identidad-c\global-externa-variantes\validar-global-no-importable.py
  pruebas-tanda-08-identidad-c\global-externa-variantes\validar.py
  pruebas-tanda-08-identidad-c\rutas-unicode\validar-equivalencias.py
  pruebas-tanda-08-identidad-c\rutas-unicode\validar-dll-unicode.py
  pruebas-tanda-08-identidad-c\rutas-unicode\validar-nombre-salida.py
  pruebas-tanda-08-identidad-c\rutas-unicode\validar.py
  pruebas-tanda-08-identidad-c\tls-dll-distintas\validar-directo-precompilado.py
  pruebas-tanda-08-identidad-c\tls-dll-distintas\validar-dos-origenes.py
  pruebas-tanda-08-identidad-c\tls-dll-distintas\validar-origen-ambiguo.py
  pruebas-tanda-08-identidad-c\tls-dll-distintas\validar-transitivas-homonimas.py
  pruebas-tanda-08-identidad-c\tls-dll-distintas\validar-variadicas-agregados.py
  pruebas-tanda-08-identidad-c\tls-dll-distintas\validar-variadicas-puntero.py
  pruebas-tanda-08-identidad-c\tls-dll-distintas\validar-variadicas.py
  pruebas-tanda-08-identidad-c\tls-dll-producto\validar.py
  pruebas-tanda-08-identidad-c\tls-dll-unicode\validar-mayusculas.py
  pruebas-tanda-08-identidad-c\tls-dll-unicode\validar.py
  pruebas-tanda-08-identidad-c\tls-sin-import-host\validar-diagnostico.py
  pruebas-tanda-08-identidad-c\tls-sin-import-host\validar.py
  pruebas-tanda-08-identidad-c\typedef-calificadores\validar-callback-const.py
  pruebas-tanda-08-identidad-c\typedef-calificadores\validar-callback-volatile.py
  pruebas-tanda-08-identidad-c\typedef-calificadores\validar-delegate-alias-calificado.py
  pruebas-tanda-08-identidad-c\typedef-calificadores\validar-firma-publica-calificada.py
  pruebas-tanda-08-identidad-c\typedef-calificadores\validar-invocacion.py
  pruebas-tanda-08-identidad-c\typedef-calificadores\validar-lambda-calificada.py
  pruebas-tanda-08-identidad-c\typedef-calificadores\validar-puntero-funcion-calificado.py
  pruebas-tanda-08-identidad-c\typedef-calificadores\validar.py
  pruebas-tanda-08-identidad-c\typedef-funcion-record\validar-enum.py
  pruebas-tanda-08-identidad-c\typedef-funcion-record\validar-invocacion.py
  pruebas-tanda-08-identidad-c\typedef-funcion-record\validar-retorno.py
  pruebas-tanda-08-identidad-c\typedef-funcion-record\validar.py
  pruebas-tanda-08-incompletos-c\validar-incompletos.py
  pruebas-tanda-08-incompletos-c\validar-opaco-consumidor.py
  pruebas-tanda-08-incompletos-diag-c\validar-diagnosticos.py
  pruebas-tanda-08-infraestructura\validar-finales-cmd.py
  pruebas-tanda-08-layout-c\validar-layout.py
  pruebas-tanda-08-layout-c\validar-alineacion-efectiva.py
  pruebas-tanda-08-layout-comparacion-c\validar-comparacion.py
  pruebas-tanda-08-punteros-c\validar-punteros.py
  pruebas-tanda-08-punteros-c\validar-puntero-a-puntero-const.py
  pruebas-tanda-08-record-c\validar-etiqueta.py
  pruebas-tanda-08-record-c\validar-mascara-bits.py
  pruebas-tanda-08-record-c\validar-miembros.py
  pruebas-tanda-08-typedef-c\validar-formas.py
  pruebas-tanda-08-typedef-c\validar-reconciliacion.py
  pruebas-tanda-08-typedef-c\validar-record-dos-encabezados.py
  pruebas-tanda-08-typedef-c\validar-typedef-productores.py
  pruebas-tanda-08-union-c\validar-uniones.py
  pruebas-tanda-08-union-c\validar-miembro-sin-etiqueta.py
  pruebas-tanda-08-void-c\validar-void.py
  validar-tanda-08-derivados.py
) do call :run_python %%T
for %%T in (
  pruebas-tanda-08-layout-clang\validar.cmd
  pruebas-tanda-08-record-template\validar.cmd
  pruebas-tanda-08-global-template\validar.cmd
  pruebas-tanda-08-alias-template\validar.cmd
  pruebas-tanda-08-funcion-template\validar.cmd
  pruebas-tanda-08-flexible-c\validar-decay.cmd
  pruebas-tanda-08-identidad-c\sombra-funcion\validar.cmd
  pruebas-tanda-08-sombras-plantilla-c\validar-o0-inicializadores.cmd
  pruebas-tanda-08-sombras-plantilla-c\validar-o0-sombras.cmd
) do call :run_script %%T
popd
if "%TASK_FAILED%"=="1" (
  echo El gate complementario de la tanda 08 contiene fallos; revisar la salida anterior.
  exit /b 1
)
echo Gate complementario de la tanda 08 terminado sin fallos.
exit /b 0

:run_python
echo === Suite %~1 ===
python "%~dp0%~1"
if errorlevel 1 (
  echo FALLO: %~1
  set "TASK_FAILED=1"
)
exit /b 0

:run_script
echo === Caso %~1 ===
call "%~dp0%~1"
if errorlevel 1 (
  echo FALLO: %~1
  set "TASK_FAILED=1"
)
exit /b 0
