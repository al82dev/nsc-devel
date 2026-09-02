@echo off
setlocal EnableExtensions
rem =====================================================================================
rem NativeSharp -- LA CORRIDA. Un comando: EL PIPELINE ENTERO, DOS VECES, UN ENLAZADOR CADA VEZ.
rem
rem     all.bat            corrida completa
rem     all.bat quick      lo que run_windows_checklist.bat entienda por rapido
rem
rem EL COMPILADOR SE CONSTRUYE UNA SOLA VEZ. nsc.exe lo produce clang-cl + lld-link a traves de
rem CMake, y eso NO depende de NSC_LLD_LINK: nsc.exe es el mismo binario en las dos pasadas, y
rem tiene que serlo -- si se reconstruyera en medio, los dos reportes podrian estar midiendo dos
rem compiladores distintos y la comparacion no diria nada. Por eso la pasada 2 llama al checklist
rem en modo `norebuild`.
rem
rem LO QUE SE EJECUTA DOS VECES ES TODO LO QUE INVOCA A nsc.exe: ctest (243 pruebas, muchas de
rem ellas enlazan de verdad), los 84 trabajos de fixtures y ejemplos, el triage de bugs, el
rem manifiesto y el reporte. Ahi es donde el enlazador cambia, y ahi es donde se mide.
rem
rem     PASADA 1   lld-link          -> _logs\           EL VEREDICTO OFICIAL
rem                                                     construye e instala nsc.exe
rem     PASADA 2   nslink.exe        -> _logs_nslink\    la misma bateria, el otro enlazador
rem                                                     SIN reconstruir: el mismo nsc.exe
rem
rem CADA PASADA TIENE LO SUYO Y NADA SE MEZCLA. Los dos directorios tienen la MISMA forma:
rem
rem     SUMMARY.txt  01_build.log  02_ctest.log  03_fixtures.log  04_examples.log
rem     05_triage_run.log  06_manifest.log  10_triage.log  TRIAGE.txt
rem     cases\  bin\  REPORTE.txt
rem
rem Asi que se leen igual, se comparan linea a linea, y ningun numero de uno puede colarse en el
rem otro. Los DOS reportes son autocontenidos: si algo fallo, el fragmento de log que lo explica
rem esta DENTRO del reporte de su pasada.
rem
rem     _logs\REPORTE.txt           <- lo que hay que abrir para nsc
rem     _logs_nslink\REPORTE.txt    <- lo que hay que abrir para NSLink
rem
rem Y una tercera salida, que no es para quien corre esto:
rem
rem     A:\nslink-devel\_inbox_nsc\   el buzon de quien desarrolla NSLink
rem
rem Sale de comparar las dos pasadas. Ver nslink_pipeline.py y
rem mkdown\95_infraestructura_dos_enlazadores.md
rem
rem -------------------------------------------------------------------------------------
rem POR QUE ESTE ARCHIVO ES ASI, Y NO COMO ESTABA
rem
rem   1. `call`. Un .bat que invoca otro .bat SIN `call` transfiere el control y NO VUELVE.
rem      Sin el, este archivo terminaba en el primer paso.
rem
rem   2. `pushd "%~dp0"`. Los pasos se invocan por nombre suelto, asi que dependian de que el
rem      directorio actual fuera el del repo. Ahora se puede llamar desde cualquier sitio y
rem      desde cualquier disco.
rem
rem   3. EL CHECKLIST BORRA _logs AL ARRANCAR (`rmdir /s /q _logs`, su linea 58). Por eso la
rem      pasada 1 se GUARDA EN OTRO NOMBRE antes de lanzar la pasada 2, y se restaura al final.
rem      Sin ese paso la segunda pasada borraria la primera y quedaria un solo reporte con la
rem      etiqueta equivocada -- que es peor que no tener ninguno.
rem
rem      Es tambien la razon de la version anterior de este archivo, que llamaba al checklist
rem      UNA sola vez: entonces no habia forma de correrlo dos veces sin perder la primera.
rem      Ahora si: se mueve el directorio entero, que en el mismo volumen es instantaneo.
rem
rem   4. NO SE LLAMA A run_bug_triage.bat POR SEPARADO. El checklist YA lo llama, en su linea
rem      334, y redirige su salida a _logs\05_triage_run.log. Llamarlo aparte lo corria dos
rem      veces y mandaba la segunda salida a la consola, donde se pierde. Como el checklist
rem      entero corre dos veces, el triage tambien -- una vez por enlazador, que es el punto.
rem
rem   5. EL ORDEN DENTRO DE CADA PASADA IMPORTA: checklist (que borra _logs y lo vuelve a
rem      llenar), luego manifiesto con su salida capturada, luego el reporte, que lo lee todo.
rem
rem   6. EL MANIFIESTO SE CORRE EN LAS DOS PASADAS aunque su resultado no dependa del
rem      enlazador. Es un no-op la segunda vez, y a cambio los dos reportes quedan COMPLETOS y
rem      con la misma forma. Un reporte al que le falta una seccion se lee distinto que uno que
rem      la tiene en verde, y comparar dos cosas que no tienen la misma forma es como no
rem      compararlas.
rem
rem   7. EL CODIGO DE SALIDA ES EL DE LA PASADA 1, Y SOLO EL DE LA PASADA 1. NSLink es otro
rem      componente, con otro dueno y su propio arbol; un fallo suyo no puede poner en rojo la
rem      corrida de nsc. Se ve en el veredicto combinado, se le entrega en su buzon, y no se
rem      confunde con esto.
rem
rem   8. NSC_LLD_LINK SE FIJA EN CADA PASADA, nunca se hereda. `nsc` elige enlazador leyendo esa
rem      variable antes de buscar lld-link en el PATH. Si alguien llamo a `nslink-env.bat` en
rem      esta ventana, sin limpiarla la pasada "oficial" enlazaria con NSLink SIN DECIRLO. No es
rem      hipotetico: paso en la primera corrida de esta infraestructura -- cinco pruebas de
rem      biblioteca de importacion en rojo y "0 regresiones" al mismo tiempo, dos resultados que
rem      no pueden ser verdad a la vez.
rem
rem CRLF OBLIGATORIO en este archivo -- cmd.exe resuelve los saltos por posicion de byte.
rem =====================================================================================
pushd "%~dp0"

if defined NSC_LLD_LINK echo [all.bat] NSC_LLD_LINK venia puesta ^(%NSC_LLD_LINK%^) -- se ignora: cada pasada fija la suya.
if defined NSC_KEEP_LINK_TEMPS echo [all.bat] NSC_KEEP_LINK_TEMPS venia puesta -- se ignora.

set "NSLINK=%NSLINK_EXE%"
if not defined NSLINK set "NSLINK=C:\native-sharp\bin\nslink.exe"

rem MODO DE LA PASADA 2. La 1 recibe lo que se paso en la linea de comandos; la 2 recibe lo mismo
rem PERO SIN CONSTRUIR, porque nsc.exe ya esta construido e instalado y tiene que ser el mismo
rem binario en las dos. `norebuild` en run_windows_checklist.bat = sin compilar, pero con la
rem sincronizacion de lib\, ctest, los 84 trabajos y el triage: exactamente la misma cobertura que
rem `full` menos el paso que no depende del enlazador.
rem
rem `quick` se propaga tal cual: su build es incremental sobre un arbol que la pasada 1 acaba de
rem dejar al dia, o sea unos segundos y ningun .obj distinto. No existe un modo "quick sin build",
rem y inventarlo aqui daria dos pasadas con cobertura distinta, que es lo unico que no se vale.
rem
rem `build` no ejecuta nada, asi que no hay segunda pasada que hacer.
set "MODO2=%~1"
if "%MODO2%"=="" set "MODO2=norebuild"
if /I "%MODO2%"=="full" set "MODO2=norebuild"

rem Restos de una corrida que murio a la mitad. Se borran ANTES de empezar: un _logs_lld de ayer
rem restaurado al final de hoy seria el peor resultado posible de todos los posibles.
if exist "_logs_lld" rmdir /s /q "_logs_lld" 2>nul
if exist "_logs_nslink" rmdir /s /q "_logs_nslink" 2>nul

rem =====================================================================================
rem PASADA 1 de 2 -- lld-link. EL VEREDICTO OFICIAL.
rem =====================================================================================
echo.
echo #####################################################################################
echo # PASADA 1 de 2 -- lld-link   ^(veredicto oficial^)
echo #####################################################################################
set "NSC_LLD_LINK="
set "NSC_KEEP_LINK_TEMPS="

call run_windows_checklist.bat %*

if not exist "_logs" mkdir "_logs"
python verify_manifest.py --clean --write > "_logs\06_manifest.log" 2>&1

python report.py
set "RC_LLD=%ERRORLEVEL%"

move "_logs" "_logs_lld" >nul
if errorlevel 1 goto :move_failed

rem =====================================================================================
rem PASADA 2 de 2 -- NSLink. La misma bateria, el otro enlazador.
rem =====================================================================================
set "RC_NSLINK=-"
if /I "%MODO2%"=="build" goto :sin_pasada2
if not exist "%NSLINK%" goto :sin_nslink

echo.
echo #####################################################################################
echo # PASADA 2 de 2 -- NSLink   ^(%NSLINK%^)
echo #        modo: %MODO2%   ^(el mismo nsc.exe de la pasada 1, sin reconstruir^)
echo #####################################################################################
set "NSC_LLD_LINK=%NSLINK%"

rem NSC_KEEP_LINK_TEMPS: `nsc` deja el directorio temporal del enlace en vez de borrarlo. Ahi
rem viven program.obj, runtime.obj y las bibliotecas de importacion de CADA invocacion, mas el
rem fichero de respuesta -- que es lo que quien desarrolla NSLink pidio por escrito
rem (nsc_actions/001) para poder reproducir un fallo sin depender de esta maquina.
rem
rem Va DENTRO de _logs para que el `move` de abajo se lo lleve de un golpe, y solo en esta
rem pasada: en la oficial seria basura sin nadie que la lea.
set "NSC_KEEP_LINK_TEMPS=%CD%\_logs\linktemps"

call run_windows_checklist.bat %MODO2%

if not exist "_logs" mkdir "_logs"
python verify_manifest.py --clean --write > "_logs\06_manifest.log" 2>&1

python report.py
set "RC_NSLINK=%ERRORLEVEL%"

set "NSC_LLD_LINK="
set "NSC_KEEP_LINK_TEMPS="
move "_logs" "_logs_nslink" >nul
if errorlevel 1 goto :move_failed
goto :restaurar

:sin_nslink
echo.
echo [all.bat] NO SE ENCONTRO %NSLINK%
echo [all.bat] La pasada 2 se omite. Se fija con: set NSLINK_EXE=^<ruta a nslink.exe^>
set "RC_NSLINK=-"
goto :restaurar

:sin_pasada2
echo.
echo [all.bat] Modo `build`: no se ejecuta nada, asi que no hay pasada 2 que hacer.
set "RC_NSLINK=-"

:restaurar
rem _logs vuelve a ser la pasada oficial, que es lo que todo el mundo y report.py esperan.
move "_logs_lld" "_logs" >nul
if errorlevel 1 goto :move_failed

rem =====================================================================================
rem EL BUZON DE NSLink. Compara las DOS pasadas -- no vuelve a compilar nada.
rem =====================================================================================
set "INBOX_LINE=no se genero"
if "%RC_NSLINK%"=="-" goto :veredicto
python nslink_pipeline.py --compare
if errorlevel 1 set "INBOX_LINE=generado, con regresiones"
if not errorlevel 1 set "INBOX_LINE=generado"

rem =====================================================================================
rem VEREDICTO
rem =====================================================================================
:veredicto
echo.
echo =====================================================================================
type "_logs\REPORTE.txt"
echo =====================================================================================
echo.
echo =====================================================================================
echo VEREDICTO COMBINADO -- EL MISMO PIPELINE, LOS DOS ENLAZADORES
echo =====================================================================================
if "%RC_LLD%"=="0" goto :v_lld_verde
echo   PASADA 1  lld-link  [OFICIAL] : CON FALLOS      -^> _logs\REPORTE.txt
goto :v_nslink
:v_lld_verde
echo   PASADA 1  lld-link  [OFICIAL] : VERDE           -^> _logs\REPORTE.txt
:v_nslink
if "%RC_NSLINK%"=="-" goto :v_nslink_omitida
if "%RC_NSLINK%"=="0" goto :v_nslink_verde
echo   PASADA 2  NSLink              : CON FALLOS      -^> _logs_nslink\REPORTE.txt
goto :v_buzon
:v_nslink_verde
echo   PASADA 2  NSLink              : VERDE           -^> _logs_nslink\REPORTE.txt
goto :v_buzon
:v_nslink_omitida
echo   PASADA 2  NSLink              : OMITIDA -- no se encontro nslink.exe
goto :v_fin
:v_buzon
echo   Buzon de NSLink               : %INBOX_LINE%
echo                                   A:\nslink-devel\_inbox_nsc\
echo.
echo   Los dos reportes tienen la MISMA forma y NO comparten un solo numero.
echo   Lo de NSLink NO cuenta como fallo de nsc: otro componente, otro dueno, otro arbol.
:v_fin
echo =====================================================================================
echo.
if "%RC_LLD%"=="0" goto :final_verde
echo NSC CON FALLOS. Todo lo necesario para leerlos esta en _logs\REPORTE.txt
goto :final
:final_verde
echo NSC EN VERDE. El detalle esta en _logs\REPORTE.txt
:final

popd
endlocal & exit /b %RC_LLD%

:move_failed
echo.
echo [all.bat] ERROR: no se pudo mover un directorio de logs.
echo [all.bat] Estado actual del arbol:
if exist "_logs"        echo [all.bat]   _logs\         existe
if exist "_logs_lld"    echo [all.bat]   _logs_lld\     existe  ^(la pasada con lld-link^)
if exist "_logs_nslink" echo [all.bat]   _logs_nslink\  existe  ^(la pasada con NSLink^)
echo [all.bat] Causa mas probable: un fichero de _logs abierto en otro programa.
echo [all.bat] Cierra lo que este mirando esos logs y vuelve a correr.
popd
endlocal & exit /b 1
