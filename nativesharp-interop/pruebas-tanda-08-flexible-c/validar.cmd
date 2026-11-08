@echo off
setlocal
rem C flexible array members imported as NativeSharp T[*]: layout, indexed access, restrictions and
rem precompiled consumers compared with Clang. Optional overrides as in pruebas-tanda-08-layout-clang.
if not defined NSC_T08_BUILD set "NSC_T08_BUILD=A:\nsc-interop-build\tanda1-ON"
if not defined NSC_T08_OUT set "NSC_T08_OUT=A:\nsc-interop-build\tanda08-flexible-c"
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
set "FILES=Api.h Api.c Restricted.h ApiRestricted.c Main.ns Access.ns Consumer.ns Producer.ns"
set "FILES=%FILES% Project.nsproj Producer.nsproj Metadata.nsproj Shared.nsproj"
set "FILES=%FILES% RestrictedConst.ns RestrictedConst.nsproj RestrictedPacked.ns RestrictedPacked.nsproj"
set "FILES=%FILES% ProbeSizeofMember.ns ProbeSizeofMember.nsproj ProbeDecay.ns ProbeDecay.nsproj"
set "FILES=%FILES% ProbeValueInts.ns ProbeValueInts.nsproj ProbeValuePacked.ns ProbeValuePacked.nsproj"
set "FILES=%FILES% PackedAccess.ns PackedProducer.ns PackedConsumer.ns PackedProducer.nsproj PackedShared.nsproj PackedMetadata.nsproj"
set "FILES=%FILES% QualifiedAccess.ns QualifiedDirect.ns QualifiedDirect.nsproj RestrictedConstWrite.ns RestrictedConstWrite.nsproj"
set "FILES=%FILES% RestrictedConstEscape.ns RestrictedConstEscape.nsproj RestrictedPackedAddress.ns RestrictedPackedAddress.nsproj"
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
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 "%DEST%\ApiRestricted.c" -o "%DEST%\restricted.obj"
if errorlevel 1 exit /b 1
clang.exe -S -emit-llvm -O1 --target=x86_64-pc-windows-msvc -std=c11 "%DEST%\ApiRestricted.c" -o "%DEST%\restricted-ref.ll"
if errorlevel 1 exit /b 1
llvm-lib.exe /nologo /out:"%DEST%\restricted.lib" "%DEST%\restricted.obj"
if errorlevel 1 exit /b 1
nsc.exe %CLANG_ROOT% --dump-project-oop-llvm "%DEST%\Project.nsproj" > "%DEST%\project.ll"
if errorlevel 1 exit /b 1
python "%VERIFY%" "%DEST%\api-ref.ll" "%DEST%\project.ll" "%DEST%\alineacion-directa.txt" "%~dp0tabla-alineacion.txt"
if errorlevel 1 exit /b 1
for %%O in (0 1 2 3) do (
  %EMIT_EXE% "%DEST%\Project.nsproj" -O%%O --no-artifact-cache -o "%DEST%\program-O%%O.exe"
  if errorlevel 1 exit /b 1
  "%DEST%\program-O%%O.exe"
  if errorlevel 1 exit /b 1
  echo Arreglo flexible C directo O%%O: resultado esperado cero.
)
for %%O in (0 1 2 3) do (
  %EMIT_LIB% "%DEST%\Producer.nsproj" -O%%O --no-artifact-cache -o "%DEST%\CFlexibleProducer.lib"
  if errorlevel 1 exit /b 1
  for %%P in (Shared Metadata) do (
    %EMIT_EXE% "%DEST%\%%P.nsproj" -O%%O --no-artifact-cache -o "%DEST%\%%P-O%%O.exe"
    if errorlevel 1 exit /b 1
    "%DEST%\%%P-O%%O.exe"
    if errorlevel 1 exit /b 1
    echo Arreglo flexible C %%P O%%O: resultado esperado cero.
  )
)
for %%P in (Shared Metadata) do (
  nsc.exe %CLANG_ROOT% --dump-project-oop-llvm "%DEST%\%%P.nsproj" > "%DEST%\%%P.ll"
  if errorlevel 1 exit /b 1
  python "%VERIFY%" "%DEST%\api-ref.ll" "%DEST%\%%P.ll" "%DEST%\alineacion-%%P.txt" "%~dp0tabla-alineacion.txt"
  if errorlevel 1 exit /b 1
)
for %%P in (RestrictedConstWrite) do (
  %EMIT_EXE% "%DEST%\%%P.nsproj" -O0 --no-artifact-cache -o "%DEST%\%%P.exe" > "%DEST%\%%P.log" 2>&1
  if not errorlevel 1 exit /b 1
  findstr /l /c:"left side of assignment is not assignable" "%DEST%\%%P.log" >nul
  if errorlevel 1 exit /b 1
  echo Restriccion %%P: rechazo esperado confirmado.
)
%EMIT_EXE% "%DEST%\RestrictedConstEscape.nsproj" -O0 --no-artifact-cache -o "%DEST%\RestrictedConstEscape.exe" > "%DEST%\RestrictedConstEscape.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /l /c:"would discard pointee constness" "%DEST%\RestrictedConstEscape.log" >nul
if errorlevel 1 exit /b 1
echo Restriccion RestrictedConstEscape: rechazo esperado confirmado.
%EMIT_EXE% "%DEST%\RestrictedPackedAddress.nsproj" -O0 --no-artifact-cache -o "%DEST%\RestrictedPackedAddress.exe" > "%DEST%\RestrictedPackedAddress.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /l /c:"operand of & must name addressable storage" "%DEST%\RestrictedPackedAddress.log" >nul
if errorlevel 1 exit /b 1
echo Restriccion RestrictedPackedAddress: rechazo esperado confirmado.
for %%O in (0 1 2 3) do (
  %EMIT_EXE% "%DEST%\RestrictedConst.nsproj" -O%%O --no-artifact-cache -o "%DEST%\const-O%%O.exe"
  if errorlevel 1 exit /b 1
  "%DEST%\const-O%%O.exe"
  if errorlevel 1 exit /b 1
  %EMIT_EXE% "%DEST%\QualifiedDirect.nsproj" -O%%O --no-artifact-cache -o "%DEST%\qualified-O%%O.exe"
  if errorlevel 1 exit /b 1
  "%DEST%\qualified-O%%O.exe"
  if errorlevel 1 exit /b 1
  %EMIT_EXE% "%DEST%\RestrictedPacked.nsproj" -O%%O --no-artifact-cache -o "%DEST%\packed-O%%O.exe"
  if errorlevel 1 exit /b 1
  "%DEST%\packed-O%%O.exe"
  if errorlevel 1 exit /b 1
  %EMIT_LIB% "%DEST%\PackedProducer.nsproj" -O%%O --no-artifact-cache -o "%DEST%\CPackedProducer.lib"
  if errorlevel 1 exit /b 1
  for %%P in (PackedShared PackedMetadata) do (
    %EMIT_EXE% "%DEST%\%%P.nsproj" -O%%O --no-artifact-cache -o "%DEST%\%%P-O%%O.exe"
    if errorlevel 1 exit /b 1
    "%DEST%\%%P-O%%O.exe"
    if errorlevel 1 exit /b 1
  )
)
for %%P in (RestrictedPacked PackedShared PackedMetadata) do (
  nsc.exe %CLANG_ROOT% --dump-project-oop-llvm "%DEST%\%%P.nsproj" > "%DEST%\%%P.ll"
  if errorlevel 1 exit /b 1
  python "%VERIFY%" "%DEST%\restricted-ref.ll" "%DEST%\%%P.ll" "%DEST%\alignment-%%P.txt" "%~dp0tabla-alineacion-packed.txt"
  if errorlevel 1 exit /b 1
)
for %%P in (QualifiedDirect PackedShared PackedMetadata) do (
  nsc.exe %CLANG_ROOT% --dump-project-oop-llvm "%DEST%\%%P.nsproj" > "%DEST%\volatile-%%P.ll"
  if errorlevel 1 exit /b 1
  python "%~dp0..\pruebas-tanda-08-volatile-campos-c\verificar_volatile_clang.py" "%DEST%\restricted-ref.ll" "%DEST%\volatile-%%P.ll" "%DEST%\volatile-%%P.txt" "%~dp0tabla-volatile-flexible.txt"
  if errorlevel 1 exit /b 1
)
for %%P in (ProbeSizeofMember ProbeDecay) do call :probe %%P
for %%O in (0 1 2 3) do (
  for %%P in (ProbeValueInts ProbeValuePacked) do (
    call :check_value %%P %%O
    if errorlevel 1 exit /b 1
  )
)
echo Arreglo flexible C comparado con Clang: resultado esperado cero.
exit /b 0
:check_value
%EMIT_EXE% "%DEST%\%1.nsproj" -O%2 --no-artifact-cache -o "%DEST%\%1-O%2.exe" > "%DEST%\%1-O%2.log" 2>&1
if errorlevel 1 exit /b 1
"%DEST%\%1-O%2.exe"
if errorlevel 1 exit /b 1
echo ABI flexible %1 O%2: resultado esperado cero.
exit /b 0
:probe
%EMIT_EXE% "%DEST%\%1.nsproj" -O0 --no-artifact-cache -o "%DEST%\%1.exe" > "%DEST%\%1.log" 2>&1
if errorlevel 1 (
  echo RESULTADO %1: compilacion rechazada, ver %DEST%\%1.log
  exit /b 0
)
"%DEST%\%1.exe"
echo RESULTADO %1: compila, codigo de salida %ERRORLEVEL%
exit /b 0
