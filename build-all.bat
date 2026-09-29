@echo off
setlocal EnableExtensions

rem =====================================================================================
rem NativeSharp -- configurar, compilar e instalar.
rem
rem RUTAS. El arbol sale de %~dp0, la carpeta donde vive ESTE archivo: se puede mover de
rem disco (D: -> A:) o de carpeta sin tocar una linea, y el script se puede invocar desde
rem cualquier directorio actual. El toolchain no vive en el repo y por tanto si es absoluto,
rem pero esta en UNA sola variable y se pasa entero a CMake en vez de dejarlo al PATH.
rem
rem CRLF OBLIGATORIO en este archivo. cmd.exe resuelve saltos por offset de bytes asumiendo
rem CRLF; con LF se desalinea y salta bloques enteros en silencio.
rem =====================================================================================

set "ROOT=%~dp0"
if "%ROOT:~-1%"=="\" set "ROOT=%ROOT:~0,-1%"
set "BUILDDIR=%ROOT%\build-clang"
set "PREFIX=C:\native-sharp-3.0"
set "LLVMROOT=C:\clang-llvm"
set "LLVM=%LLVMROOT:\=/%"
rem --- Preflight del toolchain ------------------------------------------------------
rem MEDIDO en la primera corrida en A:. El CMakeCache viejo (el de D:) tenia
rem   CMAKE_MT:FILEPATH=C:/clang-llvm/bin/llvm-mt.exe
rem porque en aquella maquina C:\clang-llvm\bin estaba en el PATH y CMake lo encontro solo.
rem Aqui no lo esta: CMake dejo CMAKE_MT-NOTFOUND, el enlace del programa de prueba fallo con
rem "no such file or directory", y la configuracion murio en `project()` con un mensaje que
rem habla del compilador y no de la herramienta que falta.
rem
rem Por eso todas las rutas del toolchain se pasan explicitas y se comprueban aqui: si falta
rem una, el script la nombra en vez de dejar que CMake lo descubra a su manera.
if not exist "%LLVMROOT%\bin\clang-cl.exe" goto :no_toolchain
if not exist "%LLVMROOT%\bin\llvm-mt.exe" goto :no_toolchain
if not exist "%LLVMROOT%\bin\lld-link.exe" goto :no_toolchain
if not exist "%LLVMROOT%\lib\LLVM-C.lib" goto :no_toolchain
rem --- Estado de build-clang\ --------------------------------------------------------
rem Dos formas de que el arbol de build no sirva, y las dos se borran:
rem
rem   AJENO. El build-clang\ que viajo desde D:\nsc-devel lleva la ruta vieja escrita
rem   dentro de CMakeCache.txt, build.ninja, CTestTestfile.cmake y de cada .obj. CMake se
rem   niega a reutilizarlo desde otra ruta. Reemplazar la ruta a mano dejaria un arbol en
rem   el que nadie puede confiar, que en este proyecto es lo peor que puede pasar: la
rem   suite es la unica red que hay.
rem
rem   A MEDIAS. Una configuracion que fallo tambien deja un CMakeCache.txt, con sus
rem   valores fallidos dentro (CMAKE_MT-NOTFOUND, por ejemplo). Reconfigurar encima
rem   arrastra esos valores. `CMAKE_CXX_COMPILER_WORKS:INTERNAL=1` solo aparece cuando la
rem   configuracion llego al final, asi que su ausencia es la senal.
rem
rem Es idempotente y cuesta dos findstr: en cuanto el arbol se configura bien una vez, no
rem vuelve a dispararse.
if not exist "%BUILDDIR%\CMakeCache.txt" goto :cache_ok
set "CACHEROOT="
for /f "tokens=2 delims==" %%A in ('findstr /B /C:"CMAKE_HOME_DIRECTORY:INTERNAL=" "%BUILDDIR%\CMakeCache.txt" 2^>nul') do set "CACHEROOT=%%A"
set "ROOTFWD=%ROOT:\=/%"
if /I not "%CACHEROOT%"=="%ROOTFWD%" goto :cache_alien
findstr /B /C:"CMAKE_CXX_COMPILER_WORKS:INTERNAL=1" "%BUILDDIR%\CMakeCache.txt" >nul 2>&1
if errorlevel 1 goto :cache_partial
goto :cache_ok

:cache_alien
echo.
echo   build-clang\ se genero desde "%CACHEROOT%" y este arbol esta en "%ROOTFWD%".
echo   Se borra y se reconfigura desde cero. La proxima compilacion es completa.
echo.
rmdir /s /q "%BUILDDIR%" 2>nul
goto :cache_ok

:cache_partial
echo.
echo   build-clang\ tiene un CMakeCache.txt de una configuracion que no llego a terminar.
echo   Se borra: reconfigurar encima arrastra sus valores fallidos.
echo.
rmdir /s /q "%BUILDDIR%" 2>nul

:cache_ok

cmake -S "%ROOT%" -B "%BUILDDIR%" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER="%LLVM%/bin/clang-cl.exe" -DCMAKE_LINKER="%LLVM%/bin/lld-link.exe" -DCMAKE_MT="%LLVM%/bin/llvm-mt.exe" -DCMAKE_RC_COMPILER="%LLVM%/bin/llvm-rc.exe" -DCMAKE_EXE_LINKER_FLAGS="-fuse-ld=lld" -DCMAKE_DISABLE_FIND_PACKAGE_LLVM=TRUE -DNSC_LLVM_MONOLITHIC_LIBRARY="%LLVM%/lib/LLVM-C.lib" -DNSC_BUILD_TESTS=ON -DNSC_WARNINGS_AS_ERRORS=ON
if errorlevel 1 goto :failed_configure

cmake --build "%BUILDDIR%" --parallel -- -k 0
if errorlevel 1 goto :failed_build

cmake --install "%BUILDDIR%" --prefix "%PREFIX%"
if errorlevel 1 goto :failed_install

echo.
echo   Compilado e instalado en %PREFIX%
goto :end

:failed_configure
echo.
echo   CMake no configuro.
goto :end

:failed_build
echo.
echo   El build fallo. Con NSC_WARNINGS_AS_ERRORS=ON, un warning nuevo tambien para aqui.
echo   Ninja va con -k 0, asi que ARRIBA estan TODOS los archivos que fallaron, no solo el
echo   primero. Para tenerlos en un archivo en vez de en la consola: run_ids_121_122.bat,
echo   que deja _logs\ids_121_122\02_build.log.
goto :end

:failed_install
echo.
echo   Compilo, pero la instalacion en %PREFIX% fallo.
goto :end

:no_toolchain
echo.
echo   Falta algo del toolchain en %LLVMROOT%
echo   Se esperan: bin\clang-cl.exe, bin\llvm-mt.exe, bin\lld-link.exe, lib\LLVM-C.lib
goto :end
:end
endlocal
