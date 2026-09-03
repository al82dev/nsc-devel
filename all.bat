@echo off
setlocal EnableExtensions
rem =====================================================================================
rem NativeSharp -- LA CORRIDA. Un comando: EL PIPELINE ENTERO, DOS VECES, UN ENLAZADOR CADA VEZ.
rem
rem     all.bat            corrida completa
rem     all.bat quick      lo que run_windows_checklist.bat entienda por rapido
rem
rem     PASADA 1   lld-link      ->  _logs_lldlink\    EL OFICIAL
rem     PASADA 2   nslink.exe    ->  _logs_nslink\
rem
rem EL COMPILADOR SE CONSTRUYE UNA SOLA VEZ, en la pasada 1, en build-clang\. La pasada 2 NO
rem reconstruye nada: corre la misma bateria contra EL MISMO nsc.exe cambiando una variable de
rem entorno. Cada pasada tiene su propio directorio de logs desde el primer byte -- eso no se
rem comparte y no se mueve nada.
rem
rem LAS DOS SE CORREN SIEMPRE. Un fallo de la 1 no cancela la 2: los dos reportes juntos son lo
rem que dice de quien es el fallo, y tener solo uno es justo lo que no sirve.
rem
rem Los dos directorios tienen la MISMA forma:
rem
rem     SUMMARY.txt  01_build.log  02_ctest.log  03_fixtures.log  04_examples.log
rem     05_triage_run.log  06_manifest.log  10_triage.log  TRIAGE.txt
rem     cases\  bin\  REPORTE.txt
rem
rem     _logs_lldlink\REPORTE.txt   <- lo que hay que abrir para nsc
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
rem   1. UN SOLO ARBOL DE BUILD, Y EL COMPILADOR SE CONSTRUYE UNA VEZ.
rem
rem      Esto estuvo al reves hasta el 2026-09-02: habia un build-clang-lldlink\ y un
rem      build-clang-nslink\, y la pasada 2 volvia a configurar, compilar los 419 objetivos e
rem      instalar. Medido en el log de esa corrida: `_logs_nslink\01_build.log` tiene las 419
rem      lineas [N/419] y la cabecera de configuracion de CMake. Un build entero, desde cero.
rem
rem      Y CONSTRUIA EXACTAMENTE EL MISMO COMPILADOR. El enlazador del C++ no depende de
rem      NSC_LLD_LINK: esta clavado en la linea de configuracion de run_windows_checklist.bat
rem      (-DCMAKE_LINKER=lld-link.exe -DCMAKE_EXE_LINKER_FLAGS=-fuse-ld=lld), asi que las dos
rem      pasadas compilaban con clang-cl y enlazaban con lld-link, las dos.
rem
rem      NSC_LLD_LINK lo lee `nsc` EN TIEMPO DE EJECUCION (findLldLink, src/Linker/LldLinker.cpp)
rem      para decidir con que enlaza los programas NativeSharp que compila. Eso es lo unico que
rem      distingue a las dos pasadas, y no necesita recompilar una sola linea de C++.
rem
rem      LO QUE SE GANA NO ES SOBRE TODO TIEMPO -- aquel build eran ~71 s de una corrida de ~20
rem      minutos. Lo que se gana es que las dos pasadas usan EL MISMO BINARIO y no dos binarios
rem      construidos por separado de las mismas fuentes. Para un pipeline cuyo trabajo entero es
rem      aislar una variable, "mismo fichero" es mejor que "mismo codigo".
rem
rem      Y NO SE PIERDE NINGUNA SEPARACION, que era el motivo original de los dos arboles.
rem      Ninguna prueba escribe dentro del arbol de build: stage48_5 monta su ManagedBuild\ bajo
rem      su raiz de evidencia, los logs van a NSC_LOG_DIR, los binarios de los trabajos a
rem      _logs_*\bin, los temporales de enlace a _logs_nslink\linktemps y la evidencia de los
rem      rojos a _evidencia\<pasada>. Todo lo que habia que separar ya lo separan otras
rem      variables; el arbol de build no aportaba separacion, aportaba un build repetido.
rem
rem      La contrapartida honesta: la pasada 2 depende de que la 1 dejara el arbol sano. Si la 1
rem      no construye, la 2 no mide -- que es exactamente lo que ya pasaba antes, porque las dos
rem      construyen las mismas fuentes y fallarian igual.
rem
rem   2. DOS DIRECTORIOS DE LOGS, TAMPOCO UNO. El checklist borra su directorio de logs al
rem      arrancar, asi que compartirlo obligaba a mover la pasada 1 a otro nombre y restaurarla
rem      al final. Ese baile costo una corrida entera: al descarrilar, la ejecucion volvio a la
rem      limpieza de arriba y borro las dos pasadas. Ahora cada una nace en su sitio.
rem
rem      run_windows_checklist.bat, run_bug_triage.bat, run_cases.py y report.py leen
rem      NSC_LOG_DIR. El checklist sigue leyendo NSC_BUILD_DIR, pero este archivo YA NO LA PONE:
rem      las dos pasadas usan build-clang\, que es el valor por omision. La variable se queda
rem      porque sirve para llamar al checklist a mano contra otro arbol.
rem
rem   3. `call`. Un .bat que invoca otro .bat SIN `call` transfiere el control y NO VUELVE.
rem
rem   4. `pushd "%~dp0"`. Se puede llamar desde cualquier sitio y desde cualquier disco.
rem
rem   5. NO SE LLAMA A run_bug_triage.bat POR SEPARADO. El checklist YA lo llama y redirige su
rem      salida a 05_triage_run.log. Como el checklist entero corre dos veces, el triage tambien
rem      -- una vez por enlazador, que es el punto.
rem
rem   6. EL ORDEN DENTRO DE CADA PASADA IMPORTA: checklist, luego manifiesto con su salida
rem      capturada, luego el reporte, que lo lee todo.
rem
rem   7. EL MANIFIESTO SE CORRE EN LAS DOS PASADAS aunque su resultado no dependa del enlazador.
rem      Es un no-op la segunda vez, y a cambio los dos reportes quedan COMPLETOS y con la misma
rem      forma. Comparar dos cosas que no tienen la misma forma es como no compararlas.
rem
rem   8. EL CODIGO DE SALIDA ES EL DE LA PASADA 1, Y SOLO EL DE LA PASADA 1. NSLink es otro
rem      componente, con otro dueno y su propio arbol; un fallo suyo no puede poner en rojo la
rem      corrida de nsc. Se ve en el veredicto combinado y se le entrega en su buzon.
rem
rem   9. NSC_LLD_LINK SE FIJA EN CADA PASADA, nunca se hereda. `nsc` elige enlazador leyendo esa
rem      variable antes de buscar lld-link en el PATH. Si alguien llamo a `nslink-env.bat` en
rem      esta ventana, sin limpiarla la pasada oficial enlazaria con NSLink SIN DECIRLO. No es
rem      hipotetico: paso, y se vio como cinco pruebas en rojo y "0 regresiones" a la vez.
rem
rem  10. EL PREFIJO DE INSTALACION ES UNO SOLO (C:\native-sharp). Las dos pasadas instalan ahi,
rem      asi que al terminar queda el nsc.exe de la pasada 2. Es correcto: salen de las mismas
rem      fuentes. Lo que NO se comparte es lo que se mide, que son los dos arboles y los dos
rem      directorios de logs.
rem
rem CRLF OBLIGATORIO en este archivo -- cmd.exe resuelve los saltos por posicion de byte.
rem
rem NO SE REESCRIBE ESTE ARCHIVO MIENTRAS CORRE. Por lo mismo: cambiarlo a mitad de corrida hace
rem que al volver de un `call` cmd siga leyendo en un offset que ya apunta a otro sitio. Paso el
rem 2026-09-02 y costo las dos pasadas de esa corrida.
rem =====================================================================================
pushd "%~dp0"

if defined NSC_LLD_LINK echo [all.bat] NSC_LLD_LINK venia puesta ^(%NSC_LLD_LINK%^) -- se ignora: cada pasada fija la suya.
if defined NSC_KEEP_LINK_TEMPS echo [all.bat] NSC_KEEP_LINK_TEMPS venia puesta -- se ignora.

set "NSLINK=%NSLINK_EXE%"
if not defined NSLINK set "NSLINK=C:\native-sharp\bin\nslink.exe"

rem =====================================================================================
rem LIMPIEZA. AQUI, Y SIN QUE NADIE TENGA QUE TECLEAR NADA ANTES.
rem
rem `all.bat` es LA CORRIDA: lo que mide sale de fuentes, no de .obj que sobrevivieron a saber
rem que ronda. Se borran los dos arboles de build, los dos directorios de logs y `_evidencia`.
rem
rem `_evidencia\` es donde las pruebas que construyen cosas de verdad dejan sus .obj, .lib,
rem ejecutables y logs del enlazador cuando FALLAN (ver NSC_TEST_EVIDENCE_DIR en
rem run_windows_checklist.bat). Va aqui dentro y no en %TEMP% para que se pueda leer desde
rem el repositorio; se borra al empezar por la misma razon que los logs: la evidencia de una
rem corrida no debe confundirse con la de la anterior.
rem
rem EXCEPCION: `quick` y `norebuild` conservan los arboles -- existen para NO reconstruir, y
rem borrarlos los dejaria sin nada que usar.
rem =====================================================================================
if exist "_logs_lldlink" rmdir /s /q "_logs_lldlink" 2>nul
if exist "_logs_nslink" rmdir /s /q "_logs_nslink" 2>nul
if exist "_evidencia" rmdir /s /q "_evidencia" 2>nul

rem Los dos arboles de la epoca de "un build por pasada" ya no los usa nadie. Se borran SIEMPRE
rem --tambien en quick y norebuild-- porque no son un arbol que conservar, son basura de varios
rem GB que ningun script vuelve a mirar.
if exist "build-clang-lldlink" rmdir /s /q "build-clang-lldlink" 2>nul
if exist "build-clang-nslink" rmdir /s /q "build-clang-nslink" 2>nul

if /I "%~1"=="quick" goto :sin_borrar_build
if /I "%~1"=="norebuild" goto :sin_borrar_build
echo [all.bat] Borrando build-clang\ -- esta corrida se construye desde cero.
if exist "build-clang" rmdir /s /q "build-clang" 2>nul
goto :build_limpio

:sin_borrar_build
echo [all.bat] Los arboles de build se conservan ^(modo %~1^).

:build_limpio

rem =====================================================================================
rem EL MODO DE LA PASADA 2.
rem
rem La pasada 2 no construye: el compilador ya lo hizo la 1. Pero tiene que medir EXACTAMENTE
rem los mismos pasos, o los dos reportes no se pueden comparar. Por eso solo se remapea `full`:
rem
rem     full      ->  norebuild    (sin build; ctest + trabajos + ejemplos, igual que la 1)
rem     quick     ->  quick        (el build es incremental y no hace nada; mismos pasos)
rem     build     ->  build        (ninguna de las dos mide nada; simetrico)
rem     norebuild ->  norebuild
rem
rem Si la pasada 2 usara `norebuild` siempre, un `all.bat quick` --que se salta ctest-- lo
rem correria en la 2 y no en la 1, y la comparacion mediria dos cosas distintas.
rem =====================================================================================
set "MODO1=%~1"
if "%MODO1%"=="" set "MODO1=full"
set "MODO2=%MODO1%"
if /I "%MODO1%"=="full" set "MODO2=norebuild"

rem =====================================================================================
rem PASADA 1 de 2 -- lld-link. EL VEREDICTO OFICIAL. AQUI SE CONSTRUYE EL COMPILADOR.
rem =====================================================================================
echo.
echo #####################################################################################
echo # PASADA 1 de 2 -- lld-link   ^(veredicto oficial^)
echo #        build-clang\  ---^>  _logs_lldlink\
echo #####################################################################################
set "NSC_LLD_LINK="
set "NSC_KEEP_LINK_TEMPS="
set "NSC_LOG_DIR=_logs_lldlink"

call run_windows_checklist.bat %*

if not exist "_logs_lldlink" mkdir "_logs_lldlink"
python verify_manifest.py --clean --write > "_logs_lldlink\06_manifest.log" 2>&1

python report.py
set "RC_LLD=%ERRORLEVEL%"

rem =====================================================================================
rem PASADA 2 de 2 -- NSLink. La misma bateria, el MISMO nsc.exe, su propio directorio.
rem =====================================================================================
set "RC_NSLINK=-"
if not exist "%NSLINK%" goto :sin_nslink

echo.
echo #####################################################################################
echo # PASADA 2 de 2 -- NSLink   ^(%NSLINK%^)
echo #        el mismo build-clang\ de la pasada 1  ---^>  _logs_nslink\
echo #####################################################################################
set "NSC_LLD_LINK=%NSLINK%"
set "NSC_LOG_DIR=_logs_nslink"

rem NSC_KEEP_LINK_TEMPS: `nsc` deja el directorio temporal del enlace en vez de borrarlo. Ahi
rem viven program.obj, runtime.obj y las bibliotecas de importacion de CADA invocacion, mas el
rem fichero de respuesta -- que es lo que quien desarrolla NSLink pidio por escrito
rem (nsc_actions/001) para poder reproducir un fallo sin depender de esta maquina.
rem Solo en esta pasada: en la oficial seria basura sin nadie que la lea.
set "NSC_KEEP_LINK_TEMPS=%CD%\_logs_nslink\linktemps"

rem Sin build: el compilador es el que dejo la pasada 1. Ver el bloque "EL MODO DE LA PASADA 2".
call run_windows_checklist.bat %MODO2%

if not exist "_logs_nslink" mkdir "_logs_nslink"
python verify_manifest.py --clean --write > "_logs_nslink\06_manifest.log" 2>&1

python report.py
set "RC_NSLINK=%ERRORLEVEL%"

set "NSC_LLD_LINK="
set "NSC_KEEP_LINK_TEMPS="
set "NSC_BUILD_DIR="
set "NSC_LOG_DIR="

rem =====================================================================================
rem EL BUZON DE NSLink. Compara las DOS pasadas -- no vuelve a compilar nada.
rem =====================================================================================
set "INBOX_LINE=no se genero"
python nslink_pipeline.py --compare
if errorlevel 1 set "INBOX_LINE=generado, con regresiones"
if not errorlevel 1 set "INBOX_LINE=generado"
goto :veredicto

:sin_nslink
echo.
echo [all.bat] NO SE ENCONTRO %NSLINK%
echo [all.bat] La pasada 2 se omite. Se fija con: set NSLINK_EXE=^<ruta a nslink.exe^>
set "RC_NSLINK=-"
set "INBOX_LINE=no se genero"
set "NSC_BUILD_DIR="
set "NSC_LOG_DIR="

rem =====================================================================================
rem VEREDICTO
rem =====================================================================================
:veredicto
echo.
echo =====================================================================================
type "_logs_lldlink\REPORTE.txt"
echo =====================================================================================
echo.
echo =====================================================================================
echo VEREDICTO COMBINADO -- EL MISMO PIPELINE, LOS DOS ENLAZADORES
echo =====================================================================================
if "%RC_LLD%"=="0" goto :v_lld_verde
echo   PASADA 1  lld-link  [OFICIAL] : CON FALLOS      -^> _logs_lldlink\REPORTE.txt
goto :v_nslink
:v_lld_verde
echo   PASADA 1  lld-link  [OFICIAL] : VERDE           -^> _logs_lldlink\REPORTE.txt
:v_nslink
if "%RC_NSLINK%"=="-" goto :v_nslink_omitida
if "%RC_NSLINK%"=="0" goto :v_nslink_verde
echo   PASADA 2  NSLink              : CON FALLOS      -^> _logs_nslink\REPORTE.txt
goto :v_buzon
:v_nslink_verde
echo   PASADA 2  NSLink              : VERDE           -^> _logs_nslink\REPORTE.txt
goto :v_buzon
:v_nslink_omitida
echo   PASADA 2  NSLink              : OMITIDA -- ver el motivo mas arriba
goto :v_fin
:v_buzon
echo   Buzon de NSLink               : %INBOX_LINE%
echo                                   A:\nslink-devel\_inbox_nsc\
echo.
echo   Cada pasada tiene su arbol de build y su directorio de logs. No comparten nada.
echo   Lo de NSLink NO cuenta como fallo de nsc: otro componente, otro dueno, otro arbol.
:v_fin
echo =====================================================================================
echo.
if "%RC_LLD%"=="0" goto :final_verde
echo NSC CON FALLOS. Todo lo necesario para leerlos esta en _logs_lldlink\REPORTE.txt
goto :final
:final_verde
echo NSC EN VERDE. El detalle esta en _logs_lldlink\REPORTE.txt
:final

popd
endlocal & exit /b %RC_LLD%
