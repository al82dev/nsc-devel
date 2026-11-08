@echo off
setlocal
rem T08-006 volatile control: volatile scalar fields of C records compared with Clang under Microsoft volatile
rem semantics. Runtime: NativeSharp writes read by Clang-compiled accessors and the reverse, neighbour integrity,
rem layout, direct, precompiled with header and metadata-only, O0-O3. IR: atomic ordering, volatility, type and
rem alignment of every access against Clang -O1 (verificar_volatile_clang.py). The runtime part runs first.
rem Optional overrides as in pruebas-tanda-08-layout-clang: NSC_T08_BUILD, NSC_T08_OUT, NSC_T08_NSLINK.
if not defined NSC_T08_BUILD set "NSC_T08_BUILD=A:\nsc-interop-build\tanda1-ON"
if not defined NSC_T08_OUT set "NSC_T08_OUT=A:\nsc-interop-build\tanda08-volatile-campos-c"
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
set "VERIFY=%~dp0verificar_volatile_clang.py"
set "FILES=Api.h Api.c Main.ns Access.ns Consumer.ns Producer.ns"
set "FILES=%FILES% Project.nsproj Producer.nsproj Metadata.nsproj Shared.nsproj"
set "FILES=%FILES% ProbeApi.h ProbeApi.c ProbeEnum.ns ProbeEnum.nsproj ProbeRecord.ns ProbeRecord.nsproj ProbeInherited.ns ProbeInherited.nsproj"
set "FILES=%FILES% ProbeOddProducer.ns ProbeOddProducer.nsproj ProbeOddConsumer.ns ProbeOddConsumer.nsproj"
for %%F in (%FILES%) do (
  copy /y "%~dp0%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 "%DEST%\Api.c" -o "%DEST%\api.obj"
if errorlevel 1 exit /b 1
clang.exe -S -emit-llvm -O1 --target=x86_64-pc-windows-msvc -std=c11 "%DEST%\Api.c" -o "%DEST%\api-ref.ll"
if errorlevel 1 exit /b 1
llvm-lib.exe /nologo /out:"%DEST%\api.lib" "%DEST%\api.obj"
if errorlevel 1 exit /b 1
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 "%DEST%\ProbeApi.c" -o "%DEST%\probe.obj"
if errorlevel 1 exit /b 1
llvm-lib.exe /nologo /out:"%DEST%\probe.lib" "%DEST%\probe.obj"
if errorlevel 1 exit /b 1
clang.exe -S -emit-llvm -O1 --target=x86_64-pc-windows-msvc -std=c11 "%DEST%\ProbeApi.c" -o "%DEST%\probe-ref.ll"
if errorlevel 1 exit /b 1
for %%O in (0 1 2 3) do (
  %EMIT_EXE% "%DEST%\Project.nsproj" -O%%O --no-artifact-cache -o "%DEST%\program-O%%O.exe"
  if errorlevel 1 exit /b 1
  "%DEST%\program-O%%O.exe"
  if errorlevel 1 exit /b 1
  echo Campos volatiles C directo O%%O: resultado esperado cero.
)
for %%O in (0 1 2 3) do (
  %EMIT_LIB% "%DEST%\Producer.nsproj" -O%%O --no-artifact-cache -o "%DEST%\CVolatileProducer.lib"
  if errorlevel 1 exit /b 1
  for %%P in (Shared Metadata) do (
    %EMIT_EXE% "%DEST%\%%P.nsproj" -O%%O --no-artifact-cache -o "%DEST%\%%P-O%%O.exe"
    if errorlevel 1 exit /b 1
    "%DEST%\%%P-O%%O.exe"
    if errorlevel 1 exit /b 1
    echo Campos volatiles C %%P O%%O: resultado esperado cero.
  )
)
for %%O in (1 2 3) do (
  nsc.exe %CLANG_ROOT% --dump-project-oop-llvm-opt "%DEST%\Project.nsproj" -O%%O > "%DEST%\project-O%%O.ll"
  if errorlevel 1 exit /b 1
)
echo Campos volatiles C en ejecucion O0-O3: resultado esperado cero.
call :probe ProbeEnum
if errorlevel 1 exit /b 1
call :probe ProbeRecord
if errorlevel 1 exit /b 1
python "%~dp0verificar_record_volatile.py" "%DEST%\probe-ref.ll" "%DEST%\ProbeRecord.ll"
if errorlevel 1 exit /b 1
call :probe ProbeInherited
if errorlevel 1 exit /b 1
%EMIT_LIB% "%DEST%\ProbeOddProducer.nsproj" -O0 --no-artifact-cache -o "%DEST%\CVolatileOddProducer.lib" > "%DEST%\ProbeOddProducer.log" 2>&1
echo RESULTADO ProbeOddProducer: biblioteca, codigo %ERRORLEVEL%
if errorlevel 1 exit /b 1
call :probe ProbeOddConsumer
if errorlevel 1 exit /b 1
python "%~dp0verificar_record_volatile.py" "%DEST%\probe-ref.ll" "%DEST%\ProbeOddConsumer.ll"
if errorlevel 1 exit /b 1
for %%O in (1 2 3) do (
  %EMIT_EXE% "%DEST%\ProbeRecord.nsproj" -O%%O --no-artifact-cache -o "%DEST%\ProbeRecord-O%%O.exe" > "%DEST%\ProbeRecord-O%%O.log" 2>&1
  if errorlevel 1 exit /b 1
  "%DEST%\ProbeRecord-O%%O.exe"
  if errorlevel 1 exit /b 1
  %EMIT_LIB% "%DEST%\ProbeOddProducer.nsproj" -O%%O --no-artifact-cache -o "%DEST%\CVolatileOddProducer.lib" > "%DEST%\ProbeOddProducer-O%%O.log" 2>&1
  if errorlevel 1 exit /b 1
  %EMIT_EXE% "%DEST%\ProbeOddConsumer.nsproj" -O%%O --no-artifact-cache -o "%DEST%\ProbeOddConsumer-O%%O.exe" > "%DEST%\ProbeOddConsumer-O%%O.log" 2>&1
  if errorlevel 1 exit /b 1
  "%DEST%\ProbeOddConsumer-O%%O.exe"
  if errorlevel 1 exit /b 1
  nsc.exe %CLANG_ROOT% --dump-project-oop-llvm-opt "%DEST%\ProbeOddConsumer.nsproj" -O%%O > "%DEST%\ProbeOddConsumer-O%%O.ll"
  if errorlevel 1 exit /b 1
  python "%~dp0verificar_record_volatile.py" "%DEST%\probe-ref.ll" "%DEST%\ProbeOddConsumer-O%%O.ll"
  if errorlevel 1 exit /b 1
)
set "IRFAIL=0"
nsc.exe %CLANG_ROOT% --dump-project-oop-llvm "%DEST%\Project.nsproj" > "%DEST%\project.ll"
if errorlevel 1 exit /b 1
python "%VERIFY%" "%DEST%\api-ref.ll" "%DEST%\project.ll" "%DEST%\volatile-directa.txt" "%~dp0tabla-volatile.txt"
if errorlevel 1 set "IRFAIL=1"
for %%O in (1 2 3) do (
  python "%VERIFY%" "%DEST%\api-ref.ll" "%DEST%\project-O%%O.ll" "%DEST%\volatile-O%%O.txt" "%~dp0tabla-volatile.txt" >nul
  if errorlevel 1 set "IRFAIL=1"
)
for %%P in (Shared Metadata) do (
  nsc.exe %CLANG_ROOT% --dump-project-oop-llvm "%DEST%\%%P.nsproj" > "%DEST%\%%P.ll"
  if errorlevel 1 exit /b 1
  python "%VERIFY%" "%DEST%\api-ref.ll" "%DEST%\%%P.ll" "%DEST%\volatile-%%P.txt" "%~dp0tabla-volatile.txt"
  if errorlevel 1 set "IRFAIL=1"
)
if "%IRFAIL%"=="1" (
  echo Campos volatiles C: el acceso difiere de Clang, ver volatile-*.txt.
  exit /b 1
)
echo Campos volatiles C comparados con Clang: resultado esperado cero.
exit /b 0
:probe
%EMIT_EXE% "%DEST%\%1.nsproj" -O0 --no-artifact-cache -o "%DEST%\%1.exe" > "%DEST%\%1.log" 2>&1
if errorlevel 1 (
  echo RESULTADO %1: compilacion rechazada, ver %DEST%\%1.log
  exit /b 1
)
"%DEST%\%1.exe"
if errorlevel 1 exit /b 1
echo RESULTADO %1: compila, codigo de salida %ERRORLEVEL%
nsc.exe %CLANG_ROOT% --dump-project-oop-llvm "%DEST%\%1.nsproj" > "%DEST%\%1.ll"
exit /b %ERRORLEVEL%
