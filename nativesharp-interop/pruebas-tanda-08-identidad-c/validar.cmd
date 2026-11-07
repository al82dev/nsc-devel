@echo off
rem T08-002: identidad de declaraciones C importadas.
rem 1. Sondas de USR con sesiones Clang reales (presencia, estabilidad, dependencia del encabezado y
rem    encabezados homonimos en directorios distintos).
rem 2. Restauracion de USR desde el NSMETA real de cinco productores, O0 a O3, de dos formas: con el
rem    modulo precompilado armado por la prueba y por la ruta completa del driver (LibraryReference).
rem 3. Encabezados homonimos compilados con nsc (typedef y static inline).
rem 4. Matriz de typedef en dos encabezados (typedefs\casos.txt): sonda de reglas y compilacion con nsc.
rem 5. Observacion: enum anonimo usado solo como tipo de campo publicado (no falla el validador).
rem 6. Plantillas publicas que usan funciones, globales y alias C: el consumidor sin plantillas debe
rem    ejecutar; los consumidores que instancian plantillas solo se observan (propuesta de metadata).
rem Las tablas casos.txt fijan el resultado ACTUAL de cada caso; con NSC_T08_PROPUESTAS=1 el validador
rem exige en su lugar el resultado PROP que debe dar la propuesta correspondiente una vez implementada.
rem Variables opcionales (valor por omision entre parentesis):
rem   NSC_T08_BUILD  carpeta de build de NSC          (A:\nsc-interop-build\tanda1-ON)
rem   NSC_T08_OUT    carpeta de salida y temporales   (A:\nsc-interop-build\tanda08-identidad-c)
rem   NSC_T08_CLANG  raiz de Clang 23.1.0             (A:\al82dev-clang)
rem   NSC_LINKER     nslink.exe ya construido         (A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe)
rem   NSC_T08_PROPUESTAS  1 para exigir los resultados PROP (sin valor: resultados ACTUAL)
setlocal
if "%NSC_T08_BUILD%"=="" set "NSC_T08_BUILD=A:\nsc-interop-build\tanda1-ON"
if "%NSC_T08_OUT%"=="" set "NSC_T08_OUT=A:\nsc-interop-build\tanda08-identidad-c"
if "%NSC_T08_CLANG%"=="" set "NSC_T08_CLANG=A:\al82dev-clang"
if "%NSC_LINKER%"=="" set "NSC_LINKER=A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
set "PATH=%NSC_T08_CLANG%\bin;%NSC_T08_BUILD%\bin;%PATH%"
set "NSC_LLVM_LIB=%NSC_T08_CLANG%\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=%NSC_T08_BUILD%\generated\NativeSharp.Runtime.Win32.lib"
set "META_FIXTURES=%~dp0..\pruebas-tanda-08-meta-c"
set "ANON_FIXTURES=%~dp0anonimos"
set "HOMONYM_FIXTURES=%~dp0homonimos"
set "TEMPLATE_FIXTURES=%~dp0plantillas"
set "TYPEDEF_FIXTURES=%~dp0typedefs"
if not exist "%NSC_T08_OUT%\temp" mkdir "%NSC_T08_OUT%\temp"
if errorlevel 1 exit /b 1
set "TEMP=%NSC_T08_OUT%\temp"
set "TMP=%TEMP%"
echo === Sondas de identidad de declaraciones C ===
nsc_cpp_c_identity_tests.exe "%NSC_T08_CLANG%\lib\clang\23" "%NSC_T08_OUT%" "%NSC_T08_CLANG%\bin\clang.exe"
if errorlevel 1 exit /b 1
for %%V in (Producer ProducerValue ProducerUnion) do (
  for %%O in (0 1 2 3) do (
    call :restore "%META_FIXTURES%" Api api CPublishedProducer %%V %%O
    if errorlevel 1 exit /b 1
  )
)
for %%V in (ProducerAnonRecord ProducerAnonKind) do (
  for %%O in (0 1 2 3) do (
    call :restore "%ANON_FIXTURES%" Anon anon CAnonProducer %%V %%O
    if errorlevel 1 exit /b 1
  )
)
call :homonyms
if errorlevel 1 exit /b 1
call :typedefs
if errorlevel 1 exit /b 1
call :observe_anonymous_field
call :templates
if errorlevel 1 exit /b 1
echo T08-002 C identity checks passed.
exit /b 0

rem :restore <fixtures> <C base name> <C library base> <producer library> <producer source> <O level>
:restore
set "DEST=%NSC_T08_OUT%\%~5-O%~6"
if not exist "%DEST%" mkdir "%DEST%"
if errorlevel 1 exit /b 1
for %%F in (%~2.h %~2.c Producer.nsproj) do (
  copy /y "%~1\%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
copy /y "%~1\%~5.ns" "%DEST%\Producer.ns" >nul
if errorlevel 1 exit /b 1
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 -O%~6 "%DEST%\%~2.c" -o "%DEST%\%~3.obj"
if errorlevel 1 exit /b 1
llvm-lib.exe /nologo /out:"%DEST%\%~3.lib" "%DEST%\%~3.obj"
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=%NSC_T08_CLANG% --emit-project-oop-lib "%DEST%\Producer.nsproj" -O%~6 --no-artifact-cache -o "%DEST%\%~4.lib" > "%DEST%\producer.log" 2>&1
if errorlevel 1 (
  type "%DEST%\producer.log"
  exit /b 1
)
nsc_cpp_c_identity_tests.exe --verify-restored-c-usr "%DEST%\%~4.nsmeta" "%DEST%\%~2.h" "%NSC_T08_CLANG%\lib\clang\23" "%NSC_T08_CLANG%\bin\clang.exe" "%DEST%"
if errorlevel 1 exit /b 1
nsc_cpp_c_identity_tests.exe --verify-driver-restored-c-usr "%DEST%\%~4.lib" "%DEST%\%~2.h" "%NSC_T08_CLANG%\lib\clang\23" "%NSC_T08_CLANG%\bin\clang.exe"
if errorlevel 1 exit /b 1
echo Restored C USRs passed %~5 O%~6.
exit /b 0

rem :homonyms compila con nsc los proyectos con encabezados same.h de directorios distintos.
:homonyms
set "DEST=%NSC_T08_OUT%\homonimos"
for %%D in (dirA dirB dirC) do (
  if not exist "%DEST%\%%D" mkdir "%DEST%\%%D"
  copy /y "%HOMONYM_FIXTURES%\%%D\same.h" "%DEST%\%%D\same.h" >nul
  if errorlevel 1 exit /b 1
)
for %%P in (AliasAB AliasAC InlineA InlineAB InlineAC) do (
  copy /y "%HOMONYM_FIXTURES%\%%P.nsproj" "%DEST%\%%P.nsproj" >nul
  if errorlevel 1 exit /b 1
  copy /y "%HOMONYM_FIXTURES%\%%P.ns" "%DEST%\%%P.ns" >nul
  if errorlevel 1 exit /b 1
)
call :homonym_runs AliasAB
if errorlevel 1 exit /b 1
call :homonym_runs InlineA
if errorlevel 1 exit /b 1
call :homonym_rejects AliasAC "conflicting C/C++ type alias 'CSameInt'"
if errorlevel 1 exit /b 1
call :homonym_rejects InlineAB "ambiguous namespace function call to 'c_same_inline'"
if errorlevel 1 exit /b 1
call :homonym_rejects InlineAC "ambiguous namespace function call to 'c_same_inline'"
if errorlevel 1 exit /b 1
exit /b 0

:homonym_runs
nsc.exe --cpp-clang-root=%NSC_T08_CLANG% --emit-project-oop-exe "%DEST%\%~1.nsproj" -O0 --no-artifact-cache -o "%DEST%\%~1.exe" > "%DEST%\%~1.log" 2>&1
if errorlevel 1 (
  type "%DEST%\%~1.log"
  exit /b 1
)
"%DEST%\%~1.exe"
if errorlevel 1 exit /b 1
echo Homonymous headers %~1 compiled and ran.
exit /b 0

:homonym_rejects
nsc.exe --cpp-clang-root=%NSC_T08_CLANG% --emit-project-oop-exe "%DEST%\%~1.nsproj" -O0 --no-artifact-cache -o "%DEST%\%~1.exe" > "%DEST%\%~1.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /L /C:"%~2" "%DEST%\%~1.log" >nul
if errorlevel 1 (
  type "%DEST%\%~1.log"
  exit /b 1
)
echo Homonymous headers %~1 rejected: %~2
exit /b 0

rem :typedefs sonda de reglas de alias y compilacion de cada par de encabezados de typedefs\casos.txt.
:typedefs
set "DEST=%NSC_T08_OUT%\typedefs"
xcopy /e /i /y /q "%TYPEDEF_FIXTURES%" "%DEST%" >nul
if errorlevel 1 exit /b 1
nsc_cpp_c_identity_tests.exe --typedef-matrix "%DEST%" "%NSC_T08_CLANG%\lib\clang\23" "%NSC_T08_CLANG%\bin\clang.exe"
if errorlevel 1 exit /b 1
for /f "usebackq eol=# tokens=1-7" %%A in ("%DEST%\casos.txt") do (
  call :typedef_row %%A %%D %%E %%F %%G
  if errorlevel 1 exit /b 1
)
exit /b 0

rem :typedef_row <caso> <X1> <XY> <PQ> <PROP>
:typedef_row
set "TYPEDEF_XY=%~3"
if "%NSC_T08_PROPUESTAS%"=="1" set "TYPEDEF_XY=%~5"
call :case "%DEST%" %~1X1 - %~2 %~2
if errorlevel 1 exit /b 1
call :case "%DEST%" %~1XY - %TYPEDEF_XY% %TYPEDEF_XY%
if errorlevel 1 exit /b 1
call :case "%DEST%" %~1PQ - %~4 %~4
exit /b %errorlevel%

rem :case <dir> <proyecto> <productor o -> <ACTUAL> <PROP>
rem Construye el productor (una vez por ejecucion), luego el proyecto, y compara el resultado.
:case
set "CASE_DIR=%~1"
set "CASE_NAME=%~2"
set "CASE_PRODUCER=%~3"
set "CASE_EXPECT=%~4"
if "%NSC_T08_PROPUESTAS%"=="1" set "CASE_EXPECT=%~5"
set "CASE_LOG=%CASE_DIR%\%CASE_NAME%.log"
if "%CASE_PRODUCER%"=="-" goto case_project
set "CASE_LOG=%CASE_DIR%\%CASE_PRODUCER%.log"
if defined CASE_FAILED_%CASE_PRODUCER% goto case_failed
if defined CASE_BUILT_%CASE_PRODUCER% goto case_project_log
nsc.exe --cpp-clang-root=%NSC_T08_CLANG% --emit-project-oop-lib "%CASE_DIR%\%CASE_PRODUCER%.nsproj" -O0 --no-artifact-cache -o "%CASE_DIR%\%CASE_PRODUCER%.lib" > "%CASE_LOG%" 2>&1
if errorlevel 1 goto case_producer_failed
set "CASE_BUILT_%CASE_PRODUCER%=1"
:case_project_log
set "CASE_LOG=%CASE_DIR%\%CASE_NAME%.log"
:case_project
nsc.exe --cpp-clang-root=%NSC_T08_CLANG% --emit-project-oop-exe "%CASE_DIR%\%CASE_NAME%.nsproj" -O0 --no-artifact-cache -o "%CASE_DIR%\%CASE_NAME%.exe" > "%CASE_LOG%" 2>&1
if errorlevel 1 goto case_failed
if not "%CASE_EXPECT%"=="runs" (
  echo FAIL: %CASE_NAME% compiled but %CASE_EXPECT% was expected.
  exit /b 1
)
"%CASE_DIR%\%CASE_NAME%.exe"
if errorlevel 1 (
  echo FAIL: %CASE_NAME% returned a nonzero exit code.
  exit /b 1
)
echo Case %CASE_NAME% compiled and ran.
exit /b 0
:case_producer_failed
set "CASE_FAILED_%CASE_PRODUCER%=1"
:case_failed
if "%CASE_EXPECT%"=="runs" (
  echo FAIL: %CASE_NAME% was expected to run.
  type "%CASE_LOG%"
  exit /b 1
)
call :case_message %CASE_EXPECT%
if not defined CASE_TEXT (
  echo FAIL: unknown expected outcome %CASE_EXPECT% for %CASE_NAME%.
  exit /b 1
)
findstr /L /C:"%CASE_TEXT%" "%CASE_LOG%" >nul
if errorlevel 1 (
  echo FAIL: %CASE_NAME% did not report: %CASE_TEXT%
  type "%CASE_LOG%"
  exit /b 1
)
echo Case %CASE_NAME% rejected as %CASE_EXPECT%: %CASE_TEXT%
exit /b 0

rem :case_message <resultado> deja en CASE_TEXT el texto de diagnostico esperado.
:case_message
set "CASE_TEXT="
if "%~1"=="fails" set "CASE_TEXT=error"
if "%~1"=="alias" set "CASE_TEXT=conflicting C/C++ type alias"
if "%~1"=="record" set "CASE_TEXT=referenced C/C++ record has conflicting or unverified definitions"
if "%~1"=="member" set "CASE_TEXT=has no compatible member named"
exit /b 0

rem :observe_anonymous_field publica un record cuyo campo es un enum anonimo; solo informa el resultado.
:observe_anonymous_field
set "DEST=%NSC_T08_OUT%\ProducerAnonLevel-O0"
if not exist "%DEST%" mkdir "%DEST%"
for %%F in (Anon.h Anon.c Producer.nsproj) do copy /y "%ANON_FIXTURES%\%%F" "%DEST%\%%F" >nul
copy /y "%ANON_FIXTURES%\ProducerAnonLevel.ns" "%DEST%\Producer.ns" >nul
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 -O0 "%DEST%\Anon.c" -o "%DEST%\anon.obj"
llvm-lib.exe /nologo /out:"%DEST%\anon.lib" "%DEST%\anon.obj"
nsc.exe --cpp-clang-root=%NSC_T08_CLANG% --emit-project-oop-lib "%DEST%\Producer.nsproj" -O0 --no-artifact-cache -o "%DEST%\CAnonProducer.lib" > "%DEST%\producer.log" 2>&1
if errorlevel 1 (
  echo OBSERVATION anonymous enum field producer failed:
  findstr /L /C:"error" "%DEST%\producer.log"
) else (
  echo OBSERVATION anonymous enum field producer succeeded.
)
exit /b 0

rem :templates productor con plantillas publicas que usan declaraciones C; ver la propuesta de metadata.
:templates
set "DEST=%NSC_T08_OUT%\plantillas"
if not exist "%DEST%" mkdir "%DEST%"
for %%F in (Gen.h Gen.c) do (
  copy /y "%TEMPLATE_FIXTURES%\%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
for %%P in (Producer ConsumerPlain ConsumerGeneric ConsumerGenericHeader ConsumerAlias) do (
  copy /y "%TEMPLATE_FIXTURES%\%%P.nsproj" "%DEST%\%%P.nsproj" >nul
  if errorlevel 1 exit /b 1
  copy /y "%TEMPLATE_FIXTURES%\%%P.ns" "%DEST%\%%P.ns" >nul
  if errorlevel 1 exit /b 1
)
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 -O0 "%DEST%\Gen.c" -o "%DEST%\gen.obj"
if errorlevel 1 exit /b 1
llvm-lib.exe /nologo /out:"%DEST%\gen.lib" "%DEST%\gen.obj"
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=%NSC_T08_CLANG% --emit-project-oop-lib "%DEST%\Producer.nsproj" -O0 --no-artifact-cache -o "%DEST%\CGenProducer.lib" > "%DEST%\producer.log" 2>&1
if errorlevel 1 (
  type "%DEST%\producer.log"
  exit /b 1
)
nsc.exe --cpp-clang-root=%NSC_T08_CLANG% --emit-project-oop-exe "%DEST%\ConsumerPlain.nsproj" -O0 --no-artifact-cache -o "%DEST%\ConsumerPlain.exe" > "%DEST%\ConsumerPlain.log" 2>&1
if errorlevel 1 (
  type "%DEST%\ConsumerPlain.log"
  exit /b 1
)
"%DEST%\ConsumerPlain.exe"
if errorlevel 1 exit /b 1
echo Template producer: consumer of non-template C uses compiled and ran.
for %%C in (ConsumerGeneric ConsumerGenericHeader ConsumerAlias) do (
  nsc.exe --cpp-clang-root=%NSC_T08_CLANG% --emit-project-oop-exe "%DEST%\%%C.nsproj" -O0 --no-artifact-cache -o "%DEST%\%%C.exe" > "%DEST%\%%C.log" 2>&1
  if errorlevel 1 (
    echo OBSERVATION %%C failed:
    findstr /L /C:"error" "%DEST%\%%C.log"
  ) else (
    echo OBSERVATION %%C compiled.
  )
)
exit /b 0
