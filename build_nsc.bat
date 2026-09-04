@echo off
setlocal EnableExtensions

rem =====================================================================================
rem NativeSharp -- RECONSTRUIR SOLO EL COMPILADOR.
rem
rem     build_nsc.bat            construye `nsc` SOLO si algo cambio; si no, no hace nada
rem     build_nsc.bat --force    construye siempre, sin consultar
rem     build_nsc.bat --check    no construye: dice si hace falta y sale 0 (al dia) o 1
rem
rem QUE CONSTRUYE, Y QUE NO. Solo el objetivo `nsc` y las bibliotecas de las que depende. NO
rem los ~40 ejecutables de prueba que ctest necesita, que es donde se va la mayor parte del
rem tiempo de un build completo. Despues instala en C:\native-sharp, que es lo que copia el
rem `nsc.exe` y el arbol `lib\` -- barato, porque no compila nada.
rem
rem Para tocar el compilador y probar UN caso a mano, esto es lo que hace falta y nada mas.
rem Cuando lo que se quiere es la bateria entera, `all.bat` sigue siendo el camino: sus
rem pruebas necesitan sus propios ejecutables, y este fichero no los construye.
rem
rem POR QUE EXISTE, SEPARADO DE all.bat. `all.bat` en modo `full` borra build-clang\ y
rem construye los 419 objetivos desde cero SIEMPRE. Para cambiar tres lineas del backend y
rem volver a mirar el IR de una fixture, eso es minutos por cada intento.
rem
rem POR QUE CONSULTA EN VEZ DE CONSTRUIR A CIEGAS. Ninja ya es incremental, asi que construir
rem de mas cuesta segundos. Lo que la consulta evita es lo contrario: creerse que esta al dia
rem cuando no lo esta. El sesgo de build_al_dia.py va hacia RECONSTRUIR ante cualquier duda,
rem porque medir contra un compilador viejo produce un verde que no vale nada -- y un verde
rem falso es peor que un rojo, porque el rojo se investiga.
rem
rem CRLF OBLIGATORIO en este archivo. cmd.exe resuelve saltos por offset de bytes asumiendo
rem CRLF; con LF se desalinea y salta bloques enteros en silencio.
rem =====================================================================================

set "ROOT=%~dp0"
if "%ROOT:~-1%"=="\" set "ROOT=%ROOT:~0,-1%"
set "BUILDDIR=%ROOT%\build-clang"
set "PREFIX=C:\native-sharp"
set "LLVMROOT=C:\clang-llvm"
set "LLVM=%LLVMROOT:\=/%"

rem --- Preflight del toolchain -------------------------------------------------------
rem Las rutas se comprueban aqui y se pasan explicitas a CMake. Sin esto, una herramienta
rem que falta se manifiesta como un error de `project()` que habla del compilador y no de la
rem herramienta -- costo una corrida entera averiguarlo la primera vez en A:.
if not exist "%LLVMROOT%\bin\clang-cl.exe" goto :no_toolchain
if not exist "%LLVMROOT%\bin\llvm-mt.exe" goto :no_toolchain
if not exist "%LLVMROOT%\bin\lld-link.exe" goto :no_toolchain
if not exist "%LLVMROOT%\lib\LLVM-C.lib" goto :no_toolchain

if /I "%~1"=="--check" goto :solo_check
if /I "%~1"=="--force" goto :construir

python "%ROOT%\build_al_dia.py" --solo-nsc
if errorlevel 1 goto :construir
echo [build_nsc] El compilador instalado esta al dia. No se construye nada.
exit /b 0

:solo_check
rem El codigo de salida ES la respuesta: 0 al dia, 1 hay que construir. Sin efectos.
python "%ROOT%\build_al_dia.py" --solo-nsc
exit /b %ERRORLEVEL%

:construir
rem --- Configurar solo si el arbol no esta listo --------------------------------------
rem `CMAKE_CXX_COMPILER_WORKS:INTERNAL=1` solo aparece cuando la configuracion llego al
rem final: su ausencia distingue "configurado" de "se intento y fallo a medias", que deja
rem un CMakeCache.txt con valores fallidos dentro.
if not exist "%BUILDDIR%\CMakeCache.txt" goto :configurar
findstr /B /C:"CMAKE_CXX_COMPILER_WORKS:INTERNAL=1" "%BUILDDIR%\CMakeCache.txt" >nul 2>&1
if errorlevel 1 goto :configurar
goto :compilar

:configurar
echo [build_nsc] Configurando %BUILDDIR% ...
cmake -S "%ROOT%" -B "%BUILDDIR%" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER="%LLVM%/bin/clang-cl.exe" -DCMAKE_LINKER="%LLVM%/bin/lld-link.exe" -DCMAKE_MT="%LLVM%/bin/llvm-mt.exe" -DCMAKE_RC_COMPILER="%LLVM%/bin/llvm-rc.exe" -DCMAKE_EXE_LINKER_FLAGS="-fuse-ld=lld" -DCMAKE_DISABLE_FIND_PACKAGE_LLVM=TRUE -DNSC_LLVM_MONOLITHIC_LIBRARY="%LLVM%/lib/LLVM-C.lib" -DNSC_BUILD_TESTS=ON -DNSC_WARNINGS_AS_ERRORS=ON
if errorlevel 1 goto :fallo_configurar

:compilar
echo [build_nsc] Compilando el objetivo `nsc` ...
cmake --build "%BUILDDIR%" --target nsc --parallel -- -k 0
if errorlevel 1 goto :fallo_compilar

echo [build_nsc] Instalando en %PREFIX% ...
cmake --install "%BUILDDIR%" --prefix "%PREFIX%"
if errorlevel 1 goto :fallo_instalar

rem COMPROBAR EL RESULTADO EN VEZ DE FIARSE DEL CODIGO DE SALIDA. `cmake --install` devuelve 0
rem aunque el binario que instala sea el de antes; ya costo una corrida entera creerselo.
python "%ROOT%\build_al_dia.py" --solo-nsc --silencioso
if errorlevel 1 goto :fallo_verificar

echo.
echo [build_nsc] Listo. `nsc.exe` en %PREFIX%\bin esta al dia.
echo [build_nsc] Los ejecutables de prueba NO se construyeron: para la bateria, all.bat.
exit /b 0

:no_toolchain
echo.
echo [build_nsc] FALTA EL TOOLCHAIN en %LLVMROOT%.
echo [build_nsc] Se esperaban bin\clang-cl.exe, bin\llvm-mt.exe, bin\lld-link.exe y lib\LLVM-C.lib
exit /b 1

:fallo_configurar
echo.
echo [build_nsc] FALLO al configurar %BUILDDIR%.
exit /b 1

:fallo_compilar
echo.
echo [build_nsc] FALLO al compilar `nsc`. NO se instalo nada: el %PREFIX% de antes sigue
echo [build_nsc] intacto, asi que lo que se mida contra el sigue siendo el compilador viejo.
exit /b 1

:fallo_instalar
echo.
echo [build_nsc] Compilo, pero fallo la instalacion en %PREFIX%.
exit /b 1

:fallo_verificar
echo.
echo [build_nsc] FALLO: la instalacion dijo que si, pero `nsc.exe` sigue siendo mas viejo que
echo [build_nsc] alguna fuente. No se puede medir nada contra el.
exit /b 1
