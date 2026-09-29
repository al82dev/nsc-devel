@echo off
setlocal EnableExtensions

rem =====================================================================================
rem NativeSharp -- barrido de rutas absolutas y estado del entorno.
rem
rem Contesta tres preguntas y deja la respuesta por escrito:
rem   1. Que archivos del arbol tienen una ruta absoluta escrita dentro, y cual.
rem   2. Si el toolchain de C:\clang-llvm tiene las cuatro piezas que la configuracion pide.
rem   3. Que hay en el PATH y en que estado esta build-clang.
rem
rem No compila, no borra nada y no toca ningun archivo del arbol. Solo lee.
rem Deja _logs\ENTORNO.txt.
rem
rem NOTA: `findstr /s` recorre subdirectorios. Se le pasa carpeta por carpeta a proposito,
rem para NO entrar en build-clang\ ni en .git\ -- el primero tiene rutas absolutas en cada
rem uno de sus 560 KB de build.ninja por diseno, y no dice nada del arbol de fuentes.
rem
rem CRLF OBLIGATORIO en este archivo.
rem =====================================================================================

set "ROOT=%~dp0"
if "%ROOT:~-1%"=="\" set "ROOT=%ROOT:~0,-1%"
set "LOGDIR=%ROOT%\_logs"
set "OUT=%LOGDIR%\ENTORNO.txt"
set "LLVMROOT=C:\clang-llvm"
set "PREFIX=C:\native-sharp-3.0"
set "BUILDDIR=%ROOT%\build-clang"

if not exist "%LOGDIR%" mkdir "%LOGDIR%"

> "%OUT%" echo NativeSharp -- barrido de rutas y entorno
>> "%OUT%" echo Fecha: %DATE% %TIME%
>> "%OUT%" echo Raiz : %ROOT%
>> "%OUT%" echo.

echo.
echo === 1/4  Referencias a la raiz vieja (cualquier disco) ===========================
>> "%OUT%" echo ==================================================================
>> "%OUT%" echo 1. Referencias absolutas a "nsc-devel" en el arbol de fuentes
>> "%OUT%" echo    Lo que se busca: "d:\nsc-devel" y "d:/nsc-devel", en cualquier caso.
>> "%OUT%" echo    Vacio = ningun archivo del arbol lleva la raiz vieja escrita dentro.
>> "%OUT%" echo ==================================================================
call :scan "d:\nsc-devel"
call :scan "d:/nsc-devel"
>> "%OUT%" echo.

echo.
echo === 2/4  Cualquier otra ruta absoluta ============================================
>> "%OUT%" echo ==================================================================
>> "%OUT%" echo 2. Otras rutas absolutas. Lo esperado y correcto es ver SOLO
>> "%OUT%" echo    C:\clang-llvm y C:\native-sharp-3.0, que son toolchain y prefijo de
>> "%OUT%" echo    instalacion y no viven en el repo. Cualquier otra cosa es un hallazgo.
>> "%OUT%" echo ==================================================================
call :scan "d:\"
call :scan "e:\"
call :scan "a:\"
>> "%OUT%" echo.

echo.
echo === 3/4  El toolchain ===========================================================
>> "%OUT%" echo ==================================================================
>> "%OUT%" echo 3. Toolchain en %LLVMROOT%
>> "%OUT%" echo ==================================================================
call :probe "%LLVMROOT%\bin\clang-cl.exe"
call :probe "%LLVMROOT%\bin\lld-link.exe"
call :probe "%LLVMROOT%\bin\llvm-mt.exe"
call :probe "%LLVMROOT%\bin\llvm-rc.exe"
call :probe "%LLVMROOT%\lib\LLVM-C.lib"
call :probe "%PREFIX%\bin\nsc.exe"
>> "%OUT%" echo.
>> "%OUT%" echo    Herramientas segun el PATH ^(informativo: la configuracion ya no depende
>> "%OUT%" echo    del PATH para nada del toolchain, pero cmake y ninja si salen de ahi^):
for %%T in (cmake.exe ninja.exe llvm-mt.exe mt.exe) do call :which %%T
>> "%OUT%" echo.

echo.
echo === 4/4  Estado de build-clang ==================================================
>> "%OUT%" echo ==================================================================
>> "%OUT%" echo 4. build-clang\
>> "%OUT%" echo ==================================================================
if not exist "%BUILDDIR%\CMakeCache.txt" goto :nocache
set "CACHEROOT="
for /f "tokens=2 delims==" %%A in ('findstr /B /C:"CMAKE_HOME_DIRECTORY:INTERNAL=" "%BUILDDIR%\CMakeCache.txt"') do set "CACHEROOT=%%A"
set "CACHEMT="
for /f "tokens=2 delims==" %%A in ('findstr /B /C:"CMAKE_MT:FILEPATH=" "%BUILDDIR%\CMakeCache.txt"') do set "CACHEMT=%%A"
>> "%OUT%" echo    generado desde : %CACHEROOT%
>> "%OUT%" echo    este arbol es  : %ROOT:\=/%
>> "%OUT%" echo    CMAKE_MT       : %CACHEMT%
findstr /B /C:"CMAKE_CXX_COMPILER_WORKS:INTERNAL=1" "%BUILDDIR%\CMakeCache.txt" >nul 2>&1
if errorlevel 1 >> "%OUT%" echo    configuracion  : NO TERMINO -- los scripts lo borraran y reconfiguraran
if not errorlevel 1 >> "%OUT%" echo    configuracion  : completa
goto :done

:nocache
>> "%OUT%" echo    no hay CMakeCache.txt: se configura desde cero en la proxima corrida.

:done
echo.
type "%OUT%"
echo.
echo   Reporte completo en %OUT%
goto :end

rem --- subrutinas ------------------------------------------------------------------
:scan
>> "%OUT%" echo.
>> "%OUT%" echo -- buscando "%~1"
findstr /s /i /m /c:"%~1" "%ROOT%\*.bat" "%ROOT%\*.cmd" "%ROOT%\*.py" "%ROOT%\*.txt" "%ROOT%\*.md" "%ROOT%\CMakeLists.txt" >> "%OUT%" 2>nul
findstr /s /i /m /c:"%~1" "%ROOT%\src\*" "%ROOT%\include\*" "%ROOT%\lib\*" >> "%OUT%" 2>nul
findstr /s /i /m /c:"%~1" "%ROOT%\tests\*" "%ROOT%\tools\*" >> "%OUT%" 2>nul
findstr /s /i /m /c:"%~1" "%ROOT%\examples\*" "%ROOT%\book_examples\*" >> "%OUT%" 2>nul
exit /b 0

:probe
if exist "%~1" >> "%OUT%" echo    [ OK ] %~1
if not exist "%~1" >> "%OUT%" echo    [FALTA] %~1
exit /b 0

:which
set "FOUND="
for %%P in (%1) do set "FOUND=%%~$PATH:P"
if defined FOUND >> "%OUT%" echo      %1 -^> %FOUND%
if not defined FOUND >> "%OUT%" echo      %1 -^> no esta en el PATH
exit /b 0

:end
endlocal
