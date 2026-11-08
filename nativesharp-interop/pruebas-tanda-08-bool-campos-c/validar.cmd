@echo off
setlocal
rem T08-006 control: C _Bool record fields (scalar, fixed array, pack(1), union overlay) compared with Clang.
rem Runtime: field size and offsets, NativeSharp writes of 1 and 0 read by C as raw byte and as _Bool,
rem C writes read by NativeSharp, neighbour integrity, O0-O3, direct, precompiled with header and metadata-only.
rem IR: memory access type and alignment against Clang -O1 (expected today: DEFECTO, NSC i1 versus Clang i8).
rem The runtime part runs first so that its result is recorded even when the IR comparison fails.
rem Optional overrides as in pruebas-tanda-08-layout-clang: NSC_T08_BUILD, NSC_T08_OUT, NSC_T08_NSLINK.
if not defined NSC_T08_BUILD set "NSC_T08_BUILD=A:\nsc-interop-build\tanda1-ON"
if not defined NSC_T08_OUT set "NSC_T08_OUT=A:\nsc-interop-build\tanda08-bool-campos-c"
if not defined NSC_T08_NSLINK set "NSC_T08_NSLINK=A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
set "DEST=%NSC_T08_OUT%"
set "PATH=A:\al82dev-clang\bin;%NSC_T08_BUILD%\bin;%PATH%"
if not exist "%DEST%" mkdir "%DEST%"
if errorlevel 1 exit /b 1
set "TMP=%DEST%"
set "TEMP=%DEST%"
set "NSC_LINKER=%NSC_T08_NSLINK%"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=%NSC_T08_BUILD%\generated\NativeSharp.Runtime.Win32.lib"
set "CLANG_ROOT=--cpp-clang-root=A:\al82dev-clang"
set "EMIT_EXE=nsc.exe %CLANG_ROOT% --emit-project-oop-exe"
set "EMIT_LIB=nsc.exe %CLANG_ROOT% --emit-project-oop-lib"
set "VERIFY=%~dp0..\pruebas-tanda-08-layout-clang\verificar_alineacion_clang.py"
set "FILES=Api.h Api.c Main.ns Access.ns Consumer.ns Producer.ns"
set "FILES=%FILES% Project.nsproj Producer.nsproj Metadata.nsproj Shared.nsproj"
set "FILES=%FILES% ProbeAddress.ns ProbeAddress.nsproj ProbeCompound.ns ProbeCompound.nsproj"
set "FILES=%FILES% ProbeGlobal.ns ProbeGlobal.nsproj ProbeVolatile.ns ProbeVolatile.nsproj"
for %%F in (%FILES%) do (
  copy /y "%~dp0%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 "%DEST%\Api.c" -o "%DEST%\api.obj"
if errorlevel 1 exit /b 1
clang.exe -S -emit-llvm -O1 --target=x86_64-pc-windows-msvc -std=c11 "%DEST%\Api.c" -o "%DEST%\api-ref.ll"
if errorlevel 1 exit /b 1
clang.exe -S -emit-llvm -O0 --target=x86_64-pc-windows-msvc -std=c11 "%DEST%\Api.c" -o "%DEST%\api-ref-O0.ll"
if errorlevel 1 exit /b 1
llvm-lib.exe /nologo /out:"%DEST%\api.lib" "%DEST%\api.obj"
if errorlevel 1 exit /b 1
for %%O in (0 1 2 3) do (
  %EMIT_EXE% "%DEST%\Project.nsproj" -O%%O --no-artifact-cache -o "%DEST%\program-O%%O.exe"
  if errorlevel 1 exit /b 1
  "%DEST%\program-O%%O.exe"
  if errorlevel 1 exit /b 1
  echo Campos _Bool C directo O%%O: resultado esperado cero.
)
for %%O in (0 1 2 3) do (
  %EMIT_LIB% "%DEST%\Producer.nsproj" -O%%O --no-artifact-cache -o "%DEST%\CBoolProducer.lib"
  if errorlevel 1 exit /b 1
  for %%P in (Shared Metadata) do (
    %EMIT_EXE% "%DEST%\%%P.nsproj" -O%%O --no-artifact-cache -o "%DEST%\%%P-O%%O.exe"
    if errorlevel 1 exit /b 1
    "%DEST%\%%P-O%%O.exe"
    if errorlevel 1 exit /b 1
    echo Campos _Bool C %%P O%%O: resultado esperado cero.
  )
)
for %%O in (0 1 2 3) do (
  nsc.exe %CLANG_ROOT% --dump-project-oop-llvm-opt "%DEST%\Project.nsproj" -O%%O > "%DEST%\project-O%%O.ll"
  if errorlevel 1 exit /b 1
)
echo Campos _Bool C en ejecucion O0-O3: resultado esperado cero.
call :probe ProbeAddress
call :probe ProbeCompound
call :probe ProbeGlobal
call :probe ProbeVolatile
if exist "%DEST%\ProbeVolatile.ll" python "%VERIFY%" "%DEST%\api-ref.ll" "%DEST%\ProbeVolatile.ll" "%DEST%\alineacion-ProbeVolatile.txt" "%~dp0tabla-volatile.txt"
echo RESULTADO ProbeVolatile IR: informativo, ver alineacion-ProbeVolatile.txt
set "IRFAIL=0"
nsc.exe %CLANG_ROOT% --dump-project-oop-llvm "%DEST%\Project.nsproj" > "%DEST%\project.ll"
if errorlevel 1 exit /b 1
python "%VERIFY%" "%DEST%\api-ref.ll" "%DEST%\project.ll" "%DEST%\alineacion-directa.txt" "%~dp0tabla-alineacion.txt"
if errorlevel 1 set "IRFAIL=1"
for %%P in (Shared Metadata) do (
  nsc.exe %CLANG_ROOT% --dump-project-oop-llvm "%DEST%\%%P.nsproj" > "%DEST%\%%P.ll"
  if errorlevel 1 exit /b 1
  python "%VERIFY%" "%DEST%\api-ref.ll" "%DEST%\%%P.ll" "%DEST%\alineacion-%%P.txt" "%~dp0tabla-alineacion.txt"
  if errorlevel 1 set "IRFAIL=1"
)
if "%IRFAIL%"=="1" (
  echo Campos _Bool C: el tipo de acceso en memoria difiere de Clang, ver alineacion-*.txt.
  exit /b 1
)
echo Campos _Bool C comparados con Clang: resultado esperado cero.
exit /b 0
:probe
%EMIT_EXE% "%DEST%\%1.nsproj" -O0 --no-artifact-cache -o "%DEST%\%1.exe" > "%DEST%\%1.log" 2>&1
if errorlevel 1 (
  echo RESULTADO %1: compilacion rechazada, ver %DEST%\%1.log
  exit /b 0
)
"%DEST%\%1.exe"
echo RESULTADO %1: compila, codigo de salida %ERRORLEVEL%
nsc.exe %CLANG_ROOT% --dump-project-oop-llvm "%DEST%\%1.nsproj" > "%DEST%\%1.ll"
exit /b 0
