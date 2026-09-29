@echo off
setlocal EnableExtensions

rem =====================================================================================
rem NativeSharp -- SOLO LAS PRUEBAS, DE CERO. El compilador no se toca.
rem
rem     test_nsc.bat            limpia todo y corre la bateria contra el nsc ya instalado
rem     test_nsc.bat quick      lo mismo, en el modo rapido de run_windows_checklist.bat
rem
rem QUE USA COMO COMPILADOR: `C:\native-sharp-3.0\bin\nsc.exe`, el que ya esta instalado. Este
rem fichero NO lo compila, NO lo instala y NO lo sustituye. Para eso esta build_nsc.bat.
rem Lo comprueba antes de medir nada: si hay fuentes mas nuevas que ese ejecutable, avisa
rem --sin abortar-- de que la corrida entera va a hablar de un compilador viejo.
rem
rem Y es el mismo que miden los 116 tests de ctest que invocan al compilador: desde el
rem 2026-09-04 el CMakeLists los apunta a NSC_TEST_COMPILER, o sea a esa misma ruta, y no
rem al binario del arbol de build. Ver mkdown/119.
rem
rem QUE SI CONSTRUYE: las pruebas, y solo las pruebas. Dos clases, y las dos se rehacen de
rem cero porque la limpieza se las lleva:
rem
rem   * los ejecutables de ctest -- programas de C++ que se compilan con clang-cl, con el
rem     objetivo `pruebas`, que EXCLUYE `nsc`;
rem   * las fixtures y los ejemplos -- programas de NativeSharp que se compilan con el
rem     `nsc.exe` de C:\native-sharp-3.0, o sea el que ya estaba instalado.
rem
rem No se llama a `cmake --install`, asi que ese `nsc.exe` sigue siendo byte a byte el mismo
rem durante toda la corrida.
rem
rem     Las dos mitades de esto las aprendi rompiendolas. La primera version no construia los
rem     ejecutables de ctest y salieron 283 "Could not find executable". La segunda los
rem     construia con `cmake --build` a secas, que reconstruye `nsc` de paso -- o sea, cambia
rem     el compilador que se esta midiendo, que es lo unico que este fichero prometia no hacer.
rem
rem QUE LIMPIA. Los logs, los ejecutables y objetos que las fixtures producen, los temporales
rem de enlace, los temporales de ctest y la cache de python. Todo lo que una corrida anterior
rem dejo: un log viejo que sobrevive es indistinguible de uno nuevo, y ya paso.
rem
rem CRLF OBLIGATORIO en este archivo. cmd.exe resuelve saltos por offset de bytes asumiendo
rem CRLF; con LF se desalinea y salta bloques enteros en silencio.
rem =====================================================================================

set "ROOT=%~dp0"
if "%ROOT:~-1%"=="\" set "ROOT=%ROOT:~0,-1%"
cd /d "%ROOT%"
set "BUILDDIR=%ROOT%\build-clang"
if not defined NS_ROOT set "NS_ROOT=C:\native-sharp-3.0"
if not defined NSC_LLVM_ROOT set "NSC_LLVM_ROOT=C:\clang-llvm"
set "PREFIX=%NS_ROOT%"
set "LLVMROOT=%NSC_LLVM_ROOT%"
set "LLVM=%LLVMROOT:\=/%"

echo.
echo #####################################################################################
echo # test_nsc.bat -- solo pruebas. El compilador es el que ya esta instalado.
echo #####################################################################################
echo.

if not exist "%PREFIX%\bin\nsc.exe" goto :sin_compilador
echo [test_nsc] Compilador: %PREFIX%\bin\nsc.exe
for %%F in ("%PREFIX%\bin\nsc.exe") do echo [test_nsc] Fecha:       %%~tF
echo.

rem --- ESTA AL DIA ESE COMPILADOR? ---------------------------------------------------
rem Este fichero NO compila nsc, y esa promesa tiene un filo: si las fuentes cambiaron
rem despues de la ultima instalacion, la bateria entera mide un compilador viejo -- y sale
rem VERDE. Un verde asi es peor que un rojo, porque nadie vuelve a mirarlo.
rem
rem NO SE ABORTA, y es deliberado: medir a proposito una version anterior es un uso
rem legitimo, y es para lo que sirve tener el build separado de las pruebas. Pero se dice,
rem y build_al_dia.py nombra el fichero concreto que quedo mas nuevo.
where python >nul 2>&1
if errorlevel 1 (
    echo [test_nsc] AVISO: no hay python en el PATH; no se comprobo si el compilador esta
    echo [test_nsc] al dia. Se mide lo que haya instalado.
    goto :tras_al_dia
)
python "%ROOT%\build_al_dia.py" --solo-nsc
if errorlevel 1 (
    echo.
    echo [test_nsc] ##############################################################
    echo [test_nsc] # AVISO: hay fuentes MAS NUEVAS que el compilador instalado.
    echo [test_nsc] # Se mide %PREFIX%\bin\nsc.exe tal como esta, no lo que hay en src\.
    echo [test_nsc] # Si no es lo que quieres: build_nsc.bat, y vuelve a lanzar esto.
    echo [test_nsc] ##############################################################
)
:tras_al_dia
echo.

rem --- EL ENTORNO HEREDADO -----------------------------------------------------------
rem NSC_BUILD_DIR apunta al arbol de build, y si viene puesta desde la sesion de cmd manda
rem sobre lo que los scripts suponen. Ya costo una corrida: con `build-clang-nslink`
rem heredada, ctest intento entrar en un arbol que esta misma limpieza acababa de borrar, y
rem el log dijo "Failed to change working directory", que no se parece a la causa.
rem `setlocal` hace que esto NO toque la variable de tu sesion.
if defined NSC_BUILD_DIR (
    echo [test_nsc] AVISO: venia NSC_BUILD_DIR=%NSC_BUILD_DIR% ; se ignora para esta corrida.
    set "NSC_BUILD_DIR="
)
set "NSC_LLD_LINK="
set "NSC_KEEP_LINK_TEMPS="
set "NSC_LOG_DIR="

rem --- LIMPIEZA ----------------------------------------------------------------------
echo [test_nsc] Limpiando salidas de corridas anteriores ...
for %%D in (_logs _logs_lldlink _logs_nslink _logs_lldlink_solo _logs_nslink_solo _evidencia _sandbox __pycache__ build-clang-lldlink build-clang-nslink) do (
    if exist "%%D" (
        echo [test_nsc]   %%D
        rmdir /s /q "%%D" 2>nul
    )
)
rem LOS EJECUTABLES DE PRUEBA YA CONSTRUIDOS. Se borran para que se vuelvan a construir
rem de cero, que es lo que "limpiar las pruebas" significa. `build-clang\` entero NO se
rem borra: ahi vive tambien el objeto del compilador, y rehacer eso es lo que este fichero
rem existe para no hacer.
if exist "%BUILDDIR%\tests" (
    echo [test_nsc]   build-clang\tests
    rmdir /s /q "%BUILDDIR%\tests" 2>nul
)
echo [test_nsc]   %%TEMP%%\nsc-cc1*  y  %%TEMP%%\nativesharp_*
for /d %%D in ("%TEMP%\nsc-cc1*") do rmdir /s /q "%%D" 2>nul
for /d %%D in ("%TEMP%\nativesharp_*") do rmdir /s /q "%%D" 2>nul
echo.

rem --- LOS EJECUTABLES DE PRUEBA -----------------------------------------------------
rem Sin `--install`: el nsc.exe de %PREFIX% no se toca. Ninja es incremental, asi que si ya
rem estan al dia esto tarda segundos.
if not exist "%BUILDDIR%\CMakeCache.txt" goto :configurar
findstr /B /C:"CMAKE_CXX_COMPILER_WORKS:INTERNAL=1" "%BUILDDIR%\CMakeCache.txt" >nul 2>&1
if errorlevel 1 goto :configurar
if not exist "%BUILDDIR%\build.ninja" goto :configurar
goto :compilar_pruebas

:configurar
echo [test_nsc] Configurando %BUILDDIR% ...
if not exist "%LLVMROOT%\bin\clang-cl.exe" goto :no_toolchain
cmake -S "%ROOT%" -B "%BUILDDIR%" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER="%LLVM%/bin/clang-cl.exe" -DCMAKE_LINKER="%LLVM%/bin/lld-link.exe" -DCMAKE_MT="%LLVM%/bin/llvm-mt.exe" -DCMAKE_RC_COMPILER="%LLVM%/bin/llvm-rc.exe" -DCMAKE_EXE_LINKER_FLAGS="-fuse-ld=lld" -DCMAKE_DISABLE_FIND_PACKAGE_LLVM=TRUE -DNSC_LLVM_MONOLITHIC_LIBRARY="%LLVM%/lib/LLVM-C.lib" -DNSC_BUILD_TESTS=ON -DNSC_WARNINGS_AS_ERRORS=ON
if errorlevel 1 goto :fallo_configurar

:compilar_pruebas
echo [test_nsc] Construyendo los ejecutables de prueba ^(sin instalar nada^) ...
rem `--target pruebas` es un objetivo agregado que declara CMakeLists.txt con TODOS los
rem ejecutables de prueba y NINGUNO del compilador. Sin el, `cmake --build` a secas
rem reconstruiria tambien `nsc` -- justo lo que build_nsc.bat genera aparte y lo que este
rem fichero no debe tocar, porque cambiaria el compilador que se esta midiendo.
cmake --build "%BUILDDIR%" --target pruebas --parallel -- -k 0
if errorlevel 1 goto :fallo_pruebas
echo [test_nsc] Ejecutables de prueba listos. %PREFIX%\bin\nsc.exe NO se ha tocado.
echo.

rem --- LA BATERIA --------------------------------------------------------------------
rem `norebuild` hace que run_windows_checklist.bat salte configurar, compilar e INSTALAR el
rem compilador. Todo lo demas --ctest, fixtures, ejemplos, triage y manifiesto-- corre igual,
rem en las dos pasadas de enlazador.
set "MODO=norebuild"
if /I "%~1"=="quick" set "MODO=quick"

call "%ROOT%\all.bat" %MODO%
set "RC=%ERRORLEVEL%"

echo.
echo [test_nsc] Terminado con codigo %RC%.
echo [test_nsc] Reportes: _logs_lldlink\REPORTE.txt  y  _logs_nslink\REPORTE.txt
exit /b %RC%

:sin_compilador
echo.
echo [test_nsc] NO HAY COMPILADOR INSTALADO en %PREFIX%\bin\nsc.exe
echo [test_nsc] Este fichero no lo compila a proposito. Para construirlo: build_nsc.bat
exit /b 1

:no_toolchain
echo.
echo [test_nsc] FALTA EL TOOLCHAIN en %LLVMROOT%.
exit /b 1

:fallo_configurar
echo.
echo [test_nsc] FALLO al configurar %BUILDDIR%.
exit /b 1

:fallo_pruebas
echo.
echo [test_nsc] FALLO al construir los ejecutables de prueba. No se corre nada: ctest sin
echo [test_nsc] sus binarios da 283 "Could not find executable", que no dice nada del compilador.
exit /b 1
