@echo off
rem T08-002: identidad de declaraciones C importadas.
rem 1. Sondas de USR con sesiones Clang reales (presencia, estabilidad, dependencia del encabezado y
rem    encabezados homonimos en directorios distintos).
rem 2. Restauracion de USR desde el NSMETA real de cinco productores, O0 a O3, de dos formas: con el
rem    modulo precompilado armado por la prueba y por la ruta completa del driver (LibraryReference).
rem 3. Encabezados homonimos compilados con nsc (typedef y static inline).
rem 4. Matriz de typedef en dos encabezados (typedefs\casos.txt): sonda de reglas y compilacion con nsc.
rem 5. Enum declarado dentro de un campo de record C, con encabezado y por biblioteca precompilada
rem    (enum-campo\casos.txt).
rem 6. Plantillas publicas que usan declaraciones C, por biblioteca precompilada (plantillas\casos.txt).
rem 7. Constantes de enum C usadas por bibliotecas (constantes-enum\casos.txt) e inspeccion del NSMETA
rem    publicado (constantes-enum\inspeccion.txt), hallazgo H7.
rem 8. Catalogo C reescrito en NSMETA 1.42-1.45 a partir de productores reales y fixture minimo 1.42
rem    (catalogo\legacy-enum-only.hex), hallazgo H8.
rem Uso: validar.cmd [seccion]. Sin argumento ejecuta todo; "constantes" o "catalogo" ejecutan solo esa
rem seccion (con la preparacion comun).
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
set "ENUM_FIELD_FIXTURES=%~dp0enum-campo"
set "ENUM_CONSTANT_FIXTURES=%~dp0constantes-enum"
set "CATALOGUE_FIXTURES=%~dp0catalogo"
if not exist "%NSC_T08_OUT%\temp" mkdir "%NSC_T08_OUT%\temp"
if errorlevel 1 exit /b 1
set "TEMP=%NSC_T08_OUT%\temp"
set "TMP=%TEMP%"
if /i "%~1"=="constantes" goto only_enum_constants
if /i "%~1"=="catalogo" goto only_catalogue_versions
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
call "%~dp0homonimos\validar-global-static.cmd"
if errorlevel 1 exit /b 1
call "%~dp0typedefs\EnumConflict\validar.cmd"
if errorlevel 1 exit /b 1
call "%~dp0typedefs\precompiled-conflict\validar.cmd"
if errorlevel 1 exit /b 1
call "%~dp0typedefs\precompiled-enum\validar.cmd"
if errorlevel 1 exit /b 1
call "%~dp0anonimos-anidados\validar.cmd"
if errorlevel 1 exit /b 1
call "%~dp0funcion-firma\validar.cmd"
if errorlevel 1 exit /b 1
call "%~dp0sombra-funcion\validar.cmd"
if errorlevel 1 exit /b 1
call "%~dp0plantilla-funcion\validar.cmd"
if errorlevel 1 exit /b 1
call "%~dp0lambda-inicializador\validar.cmd"
if errorlevel 1 exit /b 1
call "%~dp0patron-sombra\validar.cmd"
if errorlevel 1 exit /b 1
call "%~dp0inline-dos-bibliotecas\validar.cmd"
if errorlevel 1 exit /b 1
call "%~dp0static-dos-bibliotecas\validar.cmd"
if errorlevel 1 exit /b 1
call "%~dp0static-dos-bibliotecas-mutables\validar-optimizaciones.cmd"
if errorlevel 1 exit /b 1
call "%~dp0externa-dos-bibliotecas\validar.cmd"
if errorlevel 1 exit /b 1
call "%~dp0externa-dos-bibliotecas\validar.cmd" incompatible
if errorlevel 1 exit /b 1
call "%~dp0externa-dos-bibliotecas\validar.cmd" volatile
if errorlevel 1 exit /b 1
call "%~dp0externa-dos-bibliotecas\validar.cmd" volatile-compatible
if errorlevel 1 exit /b 1
call "%~dp0externa-dos-bibliotecas\validar.cmd" const
if errorlevel 1 exit /b 1
call "%~dp0externa-dos-bibliotecas\validar.cmd" const-compatible
if errorlevel 1 exit /b 1
call "%~dp0externa-dos-bibliotecas\validar.cmd" atomic
if errorlevel 1 exit /b 1
call "%~dp0externa-dos-bibliotecas\validar.cmd" atomic-compatible
if errorlevel 1 exit /b 1
call "%~dp0validar-tls-optimizaciones.cmd"
if errorlevel 1 exit /b 1
call "%~dp0tls-dll-plantillas\validar-optimizaciones.cmd"
if errorlevel 1 exit /b 1
call "%~dp0tls-dll-distintas\validar-optimizaciones.cmd"
if errorlevel 1 exit /b 1
call "%~dp0puntero-externo-dos-bibliotecas\validar.cmd" compatible
if errorlevel 1 exit /b 1
call "%~dp0puntero-externo-dos-bibliotecas\validar.cmd" const
if errorlevel 1 exit /b 1
call "%~dp0puntero-externo-dos-bibliotecas\validar.cmd" volatile
if errorlevel 1 exit /b 1
call "%~dp0externa-dos-bibliotecas\validar-dependencia-compartida.cmd"
if errorlevel 1 exit /b 1
call "%~dp0externa-dos-bibliotecas\validar-dependencia-compartida.cmd" distintas
if errorlevel 1 exit /b 1
call :typedefs
if errorlevel 1 exit /b 1
call "%~dp0records-precompiled\validar.cmd"
if errorlevel 1 exit /b 1
call :enum_field
if errorlevel 1 exit /b 1
call :templates
if errorlevel 1 exit /b 1
call :enum_constants
if errorlevel 1 exit /b 1
call :catalogue_versions
if errorlevel 1 exit /b 1
echo T08-002 C identity checks passed.
exit /b 0

:only_enum_constants
call :enum_constants
if errorlevel 1 exit /b 1
echo T08-002 C enum constant checks passed.
exit /b 0

:only_catalogue_versions
call :catalogue_versions
if errorlevel 1 exit /b 1
echo T08-002 C catalogue version checks passed.
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

rem :enum_field enum dentro de un campo de record C publicado (enum-campo\casos.txt).
:enum_field
set "DEST=%NSC_T08_OUT%\enum-campo"
call :prepare_cases "%ENUM_FIELD_FIXTURES%" Nested nested
if errorlevel 1 exit /b 1
call :cases
exit /b %errorlevel%

rem :templates plantillas publicas que usan declaraciones C (plantillas\casos.txt).
:templates
set "DEST=%NSC_T08_OUT%\plantillas"
call :prepare_cases "%TEMPLATE_FIXTURES%" Gen gen
if errorlevel 1 exit /b 1
call :cases
exit /b %errorlevel%

rem :enum_constants constantes de enum C en bibliotecas (constantes-enum\casos.txt) e inspeccion del NSMETA
rem de cada productor construido en esta ejecucion (constantes-enum\inspeccion.txt).
:enum_constants
set "DEST=%NSC_T08_OUT%\constantes-enum"
xcopy /e /i /y /q "%ENUM_CONSTANT_FIXTURES%" "%DEST%" >nul
if errorlevel 1 exit /b 1
call :cases
if errorlevel 1 exit /b 1
for /f "usebackq eol=# tokens=1-3" %%A in ("%DEST%\inspeccion.txt") do (
  call :inspect_constants %%A %%B %%C
  if errorlevel 1 exit /b 1
)
exit /b 0

rem :inspect_constants <productor> <estado-prohibido> <enumerador-requerido>
:inspect_constants
if not defined CASE_BUILT_%~1 (
  echo Inspection %~1 skipped: the producer did not build in this run.
  exit /b 0
)
nsc_cpp_c_identity_tests.exe --inspect-c-constants "%DEST%\%~1.nsmeta" %~2 %~3
exit /b %errorlevel%

rem :catalogue_versions construye productores reales y reescribe su catalogo en NSMETA 1.42-1.45.
:catalogue_versions
set "DEST=%NSC_T08_OUT%\catalogo"
xcopy /e /i /y /q "%CATALOGUE_FIXTURES%" "%DEST%" >nul
if errorlevel 1 exit /b 1
set "CATALOGUE_INPUTS="
for %%P in (PCatEnumOnly PCatRecord PCatRecordEnum PCatFloat PCatOpaque PCatFloatOpaque) do (
  call :catalogue_producer %%P
  if errorlevel 1 exit /b 1
)
nsc_cpp_c_identity_tests.exe --catalogue-version-controls "%DEST%" %CATALOGUE_INPUTS%
if errorlevel 1 exit /b 1
if not exist "%DEST%\driver-142" mkdir "%DEST%\driver-142"
if errorlevel 1 exit /b 1
copy /y "%DEST%\PCatEnumOnly.lib" "%DEST%\driver-142\PCatEnumOnly.lib" >nul
if errorlevel 1 exit /b 1
copy /y "%DEST%\variantes\PCatEnumOnly-1.42.nsmeta" "%DEST%\driver-142\PCatEnumOnly.nsmeta" >nul
if errorlevel 1 exit /b 1
copy /y "%DEST%\Consumer142.ns" "%DEST%\driver-142\Consumer142.ns" >nul
if errorlevel 1 exit /b 1
copy /y "%DEST%\Consumer142.nsproj" "%DEST%\driver-142\Consumer142.nsproj" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=%NSC_T08_CLANG% --emit-project-oop-exe "%DEST%\driver-142\Consumer142.nsproj" -O0 --no-artifact-cache -o "%DEST%\driver-142\consumer.exe"
if errorlevel 1 exit /b 1
"%DEST%\driver-142\consumer.exe"
if errorlevel 1 exit /b 1
echo Consumidor de metadata 1.42 solo con enum: resultado esperado cero.
exit /b 0

rem :catalogue_producer <productor> construye la biblioteca y anade su NSMETA a CATALOGUE_INPUTS.
:catalogue_producer
nsc.exe --cpp-clang-root=%NSC_T08_CLANG% --emit-project-oop-lib "%DEST%\%~1.nsproj" -O0 --no-artifact-cache -o "%DEST%\%~1.lib" > "%DEST%\%~1.log" 2>&1
if errorlevel 1 (
  type "%DEST%\%~1.log"
  exit /b 1
)
set "CATALOGUE_INPUTS=%CATALOGUE_INPUTS% "%DEST%\%~1.nsmeta""
exit /b 0

rem :prepare_cases <fixtures> <C base> <C library base> copia el fixture a DEST y construye la biblioteca C.
:prepare_cases
xcopy /e /i /y /q "%~1" "%DEST%" >nul
if errorlevel 1 exit /b 1
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 -O0 "%DEST%\%~2.c" -o "%DEST%\%~3.obj"
if errorlevel 1 exit /b 1
llvm-lib.exe /nologo /out:"%DEST%\%~3.lib" "%DEST%\%~3.obj"
exit /b %errorlevel%

rem :cases compila cada fila de DEST\casos.txt (proyecto productor ACTUAL PROP).
:cases
for /f "usebackq eol=# tokens=1-4" %%A in ("%DEST%\casos.txt") do (
  call :case "%DEST%" %%A %%B %%C %%D
  if errorlevel 1 exit /b 1
)
exit /b 0

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
if "%~1"=="ident" set "CASE_TEXT=unknown identifier"
if "%~1"=="ambig" set "CASE_TEXT=ambiguous namespace state reference"
if "%~1"=="type" set "CASE_TEXT=unknown type name"
if "%~1"=="field-identity" set "CASE_TEXT=C enum field dependency has no importable identity"
if "%~1"=="enum-unavailable" set "CASE_TEXT=required C enum is unavailable"
if "%~1"=="state" set "CASE_TEXT=metadata contains an incomplete or duplicate namespace state"
exit /b 0
