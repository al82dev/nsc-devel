@echo off
rem T08-002: identidad de declaraciones C importadas.
rem 1. Sondas de USR con sesiones Clang reales (presencia, estabilidad y dependencia del encabezado).
rem 2. Restauracion de USR en el consumidor desde el NSMETA real de tres productores, O0 a O3.
rem 3. Encabezados homonimos en directorios distintos compilados con nsc (typedef y static inline).
rem Variables opcionales (valor por omision entre parentesis):
rem   NSC_T08_BUILD  carpeta de build de NSC          (A:\nsc-interop-build\tanda1-ON)
rem   NSC_T08_OUT    carpeta de salida y temporales   (A:\nsc-interop-build\tanda08-identidad-c)
rem   NSC_T08_CLANG  raiz de Clang 23.1.0             (A:\al82dev-clang)
rem   NSC_LINKER     nslink.exe ya construido         (A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe)
setlocal
if "%NSC_T08_BUILD%"=="" set "NSC_T08_BUILD=A:\nsc-interop-build\tanda1-ON"
if "%NSC_T08_OUT%"=="" set "NSC_T08_OUT=A:\nsc-interop-build\tanda08-identidad-c"
if "%NSC_T08_CLANG%"=="" set "NSC_T08_CLANG=A:\al82dev-clang"
if "%NSC_LINKER%"=="" set "NSC_LINKER=A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
set "PATH=%NSC_T08_CLANG%\bin;%NSC_T08_BUILD%\bin;%PATH%"
set "NSC_LLVM_LIB=%NSC_T08_CLANG%\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=%NSC_T08_BUILD%\generated\NativeSharp.Runtime.Win32.lib"
set "FIXTURES=%~dp0..\pruebas-tanda-08-meta-c"
set "HOMONYM_FIXTURES=%~dp0homonimos"
if not exist "%NSC_T08_OUT%\temp" mkdir "%NSC_T08_OUT%\temp"
if errorlevel 1 exit /b 1
set "TEMP=%NSC_T08_OUT%\temp"
set "TMP=%TEMP%"
echo === Sondas de identidad de declaraciones C ===
nsc_cpp_c_identity_tests.exe "%NSC_T08_CLANG%\lib\clang\23" "%NSC_T08_OUT%" "%NSC_T08_CLANG%\bin\clang.exe"
if errorlevel 1 exit /b 1
for %%V in (Producer ProducerValue ProducerUnion) do (
  for %%O in (0 1 2 3) do (
    call :restore %%O %%V
    if errorlevel 1 exit /b 1
  )
)
call :homonyms
if errorlevel 1 exit /b 1
echo T08-002 C identity checks passed.
exit /b 0

:restore
set "DEST=%NSC_T08_OUT%\%~2-O%~1"
if not exist "%DEST%" mkdir "%DEST%"
if errorlevel 1 exit /b 1
for %%F in (Api.h Api.c Producer.nsproj) do (
  copy /y "%FIXTURES%\%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
copy /y "%FIXTURES%\%~2.ns" "%DEST%\Producer.ns" >nul
if errorlevel 1 exit /b 1
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 -O%~1 "%DEST%\Api.c" -o "%DEST%\api.obj"
if errorlevel 1 exit /b 1
llvm-lib.exe /nologo /out:"%DEST%\api.lib" "%DEST%\api.obj"
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=%NSC_T08_CLANG% --emit-project-oop-lib "%DEST%\Producer.nsproj" -O%~1 --no-artifact-cache -o "%DEST%\CPublishedProducer.lib"
if errorlevel 1 exit /b 1
nsc_cpp_c_identity_tests.exe --verify-restored-c-usr "%DEST%\CPublishedProducer.nsmeta" "%DEST%\Api.h" "%NSC_T08_CLANG%\lib\clang\23" "%NSC_T08_CLANG%\bin\clang.exe" "%DEST%"
if errorlevel 1 exit /b 1
echo Restored C USRs passed %~2 O%~1.
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
