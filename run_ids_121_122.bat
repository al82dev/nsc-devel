@echo off
setlocal EnableExtensions

rem =====================================================================================
rem IDs 121 y 122 -- aritmetica de constantes enteras. Bloque 1a.
rem Doc: 82_ids_121_122_aritmetica_de_constantes.md
rem
rem Que hace, en este orden y no en otro:
rem   1. Reconstruye con warnings-as-errors. La tabla kIntegralInfo nueva vive en un
rem      namespace anonimo: si quedara sin usar, /WX la caza aqui y no tres pasos despues.
rem   2. Corre las dos pruebas del frente de constantes POR SEPARADO, para que un rojo diga
rem      cual de las dos.
rem   3. Corre la suite entera. Es la unica red que hay.
rem
rem RUTAS. El arbol sale de %~dp0; el toolchain vive en una sola variable y se pasa entero
rem a CMake en vez de dejarlo al PATH.
rem
rem CRLF OBLIGATORIO en este archivo.
rem =====================================================================================

set "ROOT=%~dp0"
if "%ROOT:~-1%"=="\" set "ROOT=%ROOT:~0,-1%"
set "BUILDDIR=%ROOT%\build-clang"
set "LOGDIR=%ROOT%\_logs\ids_121_122"
set "LLVMROOT=C:\clang-llvm"
set "LLVM=%LLVMROOT:\=/%"
if not exist "%LOGDIR%" mkdir "%LOGDIR%"

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

echo.
echo === 1/4  Configurando y compilando ===============================================
cmake -S "%ROOT%" -B "%BUILDDIR%" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER="%LLVM%/bin/clang-cl.exe" -DCMAKE_LINKER="%LLVM%/bin/lld-link.exe" -DCMAKE_MT="%LLVM%/bin/llvm-mt.exe" -DCMAKE_RC_COMPILER="%LLVM%/bin/llvm-rc.exe" -DCMAKE_EXE_LINKER_FLAGS="-fuse-ld=lld" -DCMAKE_DISABLE_FIND_PACKAGE_LLVM=TRUE -DNSC_LLVM_MONOLITHIC_LIBRARY="%LLVM%/lib/LLVM-C.lib" -DNSC_BUILD_TESTS=ON -DNSC_WARNINGS_AS_ERRORS=ON > "%LOGDIR%\01_configure.log" 2>&1
if errorlevel 1 goto :configure_failed

cmake --build "%BUILDDIR%" --parallel -- -k 0 > "%LOGDIR%\02_build.log" 2>&1
if errorlevel 1 goto :build_failed

echo.
echo === 2/4  El evaluador entero tipado (regresion CC1-102..104) =====================
ctest --test-dir "%BUILDDIR%" -R "nsc.cc1.integer-evaluator" --output-on-failure > "%LOGDIR%\03_integer_evaluator.log" 2>&1
set RC_EVAL=%errorlevel%
type "%LOGDIR%\03_integer_evaluator.log"

echo.
echo === 3/4  Correctness CC1: c17 nueva, mas c1 y c9 de regresion ====================
ctest --test-dir "%BUILDDIR%" -R "nsc.cc1.correctness" --output-on-failure > "%LOGDIR%\04_cc1_correctness.log" 2>&1
set RC_CC1=%errorlevel%
type "%LOGDIR%\04_cc1_correctness.log"

echo.
echo === 4/4  La suite entera ========================================================
ctest --test-dir "%BUILDDIR%" --output-on-failure > "%LOGDIR%\05_suite.log" 2>&1
set RC_ALL=%errorlevel%

echo.
echo =================================================================================
if "%RC_EVAL%"=="0" (echo   integer-evaluator .... VERDE) else (echo   integer-evaluator .... ROJO)
if "%RC_CC1%"=="0"  (echo   cc1.correctness ...... VERDE) else (echo   cc1.correctness ...... ROJO)
if "%RC_ALL%"=="0"  (echo   suite completa ....... VERDE) else (echo   suite completa ....... ROJO)
echo.
echo   Logs en %LOGDIR%
echo =================================================================================
echo.
echo Resumen de la suite:
findstr /C:"tests passed" /C:"tests failed" /C:"Total Test time" "%LOGDIR%\05_suite.log"
echo.
echo Fallos, si los hubo:
findstr /C:"(Failed)" /C:"(Timeout)" "%LOGDIR%\05_suite.log"
goto :end

:configure_failed
echo.
echo   CMake no configuro. Ver %LOGDIR%\01_configure.log
findstr /C:"CMake Error" /C:"NOTFOUND" "%LOGDIR%\01_configure.log"
goto :end

:build_failed
echo.
echo   El build fallo. Ver %LOGDIR%\02_build.log
echo   Con warnings-as-errors activo, un warning nuevo tambien para aqui.
findstr /C:"error" "%LOGDIR%\02_build.log"
goto :end

:no_toolchain
echo.
echo   Falta algo del toolchain en %LLVMROOT%
echo   Se esperan: bin\clang-cl.exe, bin\llvm-mt.exe, bin\lld-link.exe, lib\LLVM-C.lib
goto :end
:end
endlocal
