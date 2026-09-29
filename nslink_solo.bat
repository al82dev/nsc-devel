@echo off
setlocal EnableExtensions
rem =====================================================================================
rem NativeSharp -- CICLO CORTO: SOLO CAMBIO nslink.exe.
rem
rem     nslink_solo.bat            todo: ctest + los 84 trabajos
rem     nslink_solo.bat ctest      solo ctest    (donde viven los fallos del enlazador)
rem     nslink_solo.bat casos      solo los 84 trabajos
rem
rem QUE HACE, Y SOBRE TODO QUE NO HACE.
rem
rem   NO reconstruye desde cero ni reconfigura. Hace un build INCREMENTAL del arbol que ya existe
rem   --si no cambio nada, no cuesta nada-- porque medir con binarios de pruebas viejos no es
rem   rapido, es inutil. Si ese build recompila el COMPILADOR, lo avisa: entonces la referencia
rem   es de otro nsc.exe y lo que hay que correr es `all.bat`.
rem
rem   NO corre la pasada con lld-link. Esa ya esta hecha y guardada en `_logs_lldlink\`, de la
rem   ultima corrida completa. Aqui se USA como referencia, no se vuelve a medir: enlazar con
rem   lld-link otra vez con el mismo nsc.exe y las mismas fuentes daria exactamente lo mismo.
rem
rem   NO escribe en `_logs_lldlink\` ni en `_logs_nslink\`. Escribe en `_logs_nslink_solo\`.
rem   Esos dos son la ultima corrida completa y son comparables ENTRE SI; pisar uno los dejaria
rem   siendo dos medidas de dos momentos distintos con la misma cara, que es peor que borrarlos.
rem
rem   NO construye nada de NSLink. Eso es del otro arbol y de su dueno.
rem
rem LO QUE SI HACE:
rem
rem     0. build INCREMENTAL del arbol de pruebas   -> _logs_nslink_solo\00_build.log
rem     1. ctest ENTERO enlazando con nslink.exe   -> _logs_nslink_solo\02_ctest.log
rem     2. los 84 trabajos con nslink.exe          -> _logs_nslink_solo\cases\
rem     3. la comparacion contra _logs_lldlink     -> _logs_nslink_solo\COMPARACION_CON_LLD.txt
rem                                                -> A:\nslink-devel\_inbox_nsc\
rem
rem POR QUE ctest Y NO SOLO LOS 84. Porque los 84 trabajos PASAN con los dos enlazadores, y han
rem pasado siempre. Los fallos del enlazador --los dieciocho checks de biblioteca de importacion
rem del 2026-09-02, y los dos que quedan-- viven en ctest, que ademas de ejecutar CONSTRUYE DLLs,
rem bibliotecas de importacion y enlaza anfitriones de C y C++ contra ellas. Un ciclo corto que
rem mirara solo los 84 diria "todo bien" siempre.
rem
rem SIN REFERENCIA NO HAY CICLO. Si no existe `_logs_lldlink\`, este script se para. "84 OK" sin
rem nada contra que compararlo no dice si algo EMPEORO, que es lo unico que se quiere saber aqui.
rem
rem CRLF OBLIGATORIO -- cmd.exe resuelve los saltos por posicion de byte.
rem =====================================================================================
pushd "%~dp0"

set "MODO=%~1"
if "%MODO%"=="" set "MODO=todo"

if not defined NS_ROOT set "NS_ROOT=C:\native-sharp-3.0"
set "NSLINK=%NSLINK_EXE%"
if not defined NSLINK set "NSLINK=%NS_ROOT%\bin\nslink.exe"

rem NOMBRES CON PREFIJO, Y NO ES ESTILO.
rem
rem Estas variables las HEREDAN los procesos hijos, y MSBuild convierte TODA variable de entorno en
rem una propiedad suya, con los nombres INSENSIBLES A MAYUSCULAS. Este fichero se llamaba antes
rem `OUTDIR`, que MSBuild lee como `OutDir` -- la propiedad que fija el directorio de salida.
rem
rem Consecuencia, medida el 2026-09-02: `nsc.stage48_5` hace `dotnet build` de un proyecto C#, y
rem el .dll salia en `ManagedBuild\_logs_nslink_solo\` en vez de `bin\Release\net9.0\`. El build
rem decia "Build succeeded", el test buscaba el .dll donde toca, no estaba, y el fallo viajaba al
rem buzon de NSLink como regresion SUYA. No fallaba en all.bat, que no define esa variable.
rem
rem Un script que exporta nombres genericos al entorno de sus hijos rompe herramientas que no
rem conoce. El prefijo no es cosmetica: es lo que impide volver a hacerlo.
set "SOLO_OUT=_logs_nslink_solo"
set "REFDIR=_logs_lldlink"

rem --- Lo que tiene que existir antes de medir nada -----------------------------------
if not exist "%NSLINK%" goto :sin_nslink
if not exist "%REFDIR%\cases\results.json" goto :sin_referencia

rem El arbol de build de donde sale ctest. NO se reconstruye: se usa el que dejo la ultima
rem corrida completa. Desde el 2026-09-02 hay UNO SOLO -- all.bat construye el compilador una
rem vez y las dos pasadas lo comparten. Los dos nombres viejos se siguen aceptando por si queda
rem un arbol de antes del cambio.
set "SOLO_BUILD=build-clang"
if not exist "%SOLO_BUILD%\CTestTestfile.cmake" set "SOLO_BUILD=build-clang-lldlink"
if not exist "%SOLO_BUILD%\CTestTestfile.cmake" set "SOLO_BUILD=build-clang-nslink"
if not exist "%SOLO_BUILD%\CTestTestfile.cmake" goto :sin_build

set "CTEST_JOBS=%NUMBER_OF_PROCESSORS%"
if not defined CTEST_JOBS set "CTEST_JOBS=8"

echo.
echo #####################################################################################
echo # CICLO CORTO -- solo NSLink
echo #     enlazador : %NSLINK%
echo #     compilador: el instalado, SIN reconstruir
echo #     build     : %SOLO_BUILD%\   ^(sin recompilar^)
echo #     referencia: %REFDIR%\     ^(la ultima corrida completa^)
echo #     salida    : %SOLO_OUT%\
echo #####################################################################################

if exist "%SOLO_OUT%" rmdir /s /q "%SOLO_OUT%" 2>nul
mkdir "%SOLO_OUT%"

rem NSC_LLD_LINK: `nsc` lee esta variable antes de buscar lld-link en el PATH. Es lo unico que
rem hace falta para cambiar de enlazador -- no se recompila nada.
set "NSC_LLD_LINK=%NSLINK%"
rem Los temporales del enlace se conservan: ahi quedan program.obj, runtime.obj, las .lib y el
rem fichero de respuesta de CADA invocacion, que es con lo que se reproduce un fallo sin esta
rem maquina (peticion 001 de NSLink).
rem OJO: linktemps_ctest y NO linktemps. El paso 2 (nslink_pipeline.py) BORRA linktemps\ entero
rem al arrancar, asi que compartir el nombre se lleva por delante los temporales de ctest --
rem justo los de las pruebas que fallan, que son los unicos que hacen falta.
set "NSC_KEEP_LINK_TEMPS=%CD%\%SOLO_OUT%\linktemps_ctest"
rem Que nslink_pipeline.py escriba aqui y no en el directorio de la corrida completa.
set "NSLINK_OUT_DIR=%SOLO_OUT%"

set "RC_CTEST=0"
set "RC_CASOS=0"

if /I "%MODO%"=="casos" goto :casos

rem =====================================================================================
rem 0. BUILD INCREMENTAL. Sin esto, ctest corre los binarios de pruebas ANTERIORES.
rem
rem Paso el 2026-09-02: se cambiaron seis pruebas, se lanzo este ciclo, y midio los ejecutables
rem viejos -- las mismas dos rojas de antes, con el cambio ya en disco y sin compilar. Un ciclo
rem corto que mide binarios que no corresponden a las fuentes no es rapido, es inutil.
rem
rem Ninja no hace nada si nada cambio, asi que cuando de verdad solo cambio nslink.exe esto cuesta
rem menos de un segundo. Y si SI recompila el compilador, lo dice: entonces la referencia de
rem _logs_lldlink\ es de otro nsc.exe y hay que correr all.bat, no esto.
rem =====================================================================================
echo.
echo [0/3] Build incremental de %SOLO_BUILD%\ ...
cmake --build "%SOLO_BUILD%" --parallel > "%SOLO_OUT%\00_build.log" 2>&1
if errorlevel 1 goto :build_fallo
findstr /C:"nsc_driver" /C:"nsc_frontend" /C:"nsc_llvm" /C:"nsc_linker" "%SOLO_OUT%\00_build.log" >nul 2>&1
if not errorlevel 1 echo       AVISO: se recompilo el COMPILADOR. La referencia es de otro nsc.exe -- corre all.bat.

rem =====================================================================================
rem 1. ctest -- donde viven de verdad los fallos del enlazador
rem =====================================================================================
echo.
echo [1/3] ctest enlazando con NSLink ^(-j %CTEST_JOBS%^) ...
ctest --test-dir "%SOLO_BUILD%" --output-on-failure -j %CTEST_JOBS% > "%SOLO_OUT%\02_ctest.log" 2>&1
set "RC_CTEST=%ERRORLEVEL%"
if "%RC_CTEST%"=="0" echo       verde
if not "%RC_CTEST%"=="0" echo       CON FALLOS -- ver %SOLO_OUT%\02_ctest.log
if /I "%MODO%"=="ctest" goto :comparar

:casos
rem =====================================================================================
rem 2. Los 84 trabajos
rem =====================================================================================
echo.
echo [2/3] Los 84 trabajos con NSLink ...
python nslink_pipeline.py
set "RC_CASOS=%ERRORLEVEL%"

:comparar
rem =====================================================================================
rem 3. La comparacion contra la ultima corrida con lld-link, y el buzon
rem =====================================================================================
echo.
echo [3/3] Comparando contra %REFDIR%\ ...
python nslink_pipeline.py --compare
set "RC_CMP=%ERRORLEVEL%"

set "NSC_LLD_LINK="
set "NSC_KEEP_LINK_TEMPS="
set "NSLINK_OUT_DIR="

echo.
echo =====================================================================================
echo CICLO CORTO -- VEREDICTO
echo =====================================================================================
if /I "%MODO%"=="casos" goto :v_casos
if "%RC_CTEST%"=="0" goto :v_ctest_verde
echo   ctest con NSLink   : CON FALLOS   -^> %SOLO_OUT%\02_ctest.log
goto :v_casos
:v_ctest_verde
echo   ctest con NSLink   : VERDE
:v_casos
if /I "%MODO%"=="ctest" goto :v_cmp
if "%RC_CASOS%"=="0" goto :v_casos_verde
echo   84 trabajos        : CON FALLOS   -^> %SOLO_OUT%\REPORTE_NSLINK.txt
goto :v_cmp
:v_casos_verde
echo   84 trabajos        : VERDE
:v_cmp
echo   Comparacion        : %SOLO_OUT%\COMPARACION_CON_LLD.txt
echo   Buzon              : A:\nslink-devel\_inbox_nsc\
echo.
echo   Referencia: %REFDIR%\, de la ultima corrida completa. nsc.exe NO se reconstruyo.
echo   Si lo que cambio es el COMPILADOR, esto no vale: hay que correr all.bat.
echo =====================================================================================
popd
endlocal & exit /b %RC_CMP%

:build_fallo
echo.
echo [nslink_solo] EL BUILD INCREMENTAL FALLO -- ver %SOLO_OUT%\00_build.log
echo [nslink_solo] No se mide nada: correr ctest ahora daria el resultado de los binarios viejos.
set "NSC_LLD_LINK="
set "NSC_KEEP_LINK_TEMPS="
set "NSLINK_OUT_DIR="
popd
endlocal & exit /b 2

:sin_nslink
echo.
echo [nslink_solo] NO SE ENCONTRO %NSLINK%
echo [nslink_solo] Se fija con: set NSLINK_EXE=^<ruta a nslink.exe^>
popd
endlocal & exit /b 2

:sin_referencia
echo.
echo [nslink_solo] NO HAY REFERENCIA: falta %REFDIR%\cases\results.json
echo [nslink_solo]
echo [nslink_solo] Este ciclo compara contra la ultima corrida con lld-link. Sin ella se puede
echo [nslink_solo] decir si algo pasa o no, pero NO si algo EMPEORO -- que es lo unico que se
echo [nslink_solo] quiere saber cuando solo cambio el enlazador.
echo [nslink_solo]
echo [nslink_solo] Corre all.bat una vez y despues este cuantas veces haga falta.
popd
endlocal & exit /b 2

:sin_build
echo.
echo [nslink_solo] NO HAY ARBOL DE BUILD con pruebas registradas.
echo [nslink_solo] Se busco CTestTestfile.cmake en build-clang\ ^(y en los dos nombres viejos^).
echo [nslink_solo] Corre all.bat una vez para tenerlo.
popd
endlocal & exit /b 2
