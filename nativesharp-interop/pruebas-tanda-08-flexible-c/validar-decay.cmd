@echo off
setlocal EnableDelayedExpansion
set "DEST=A:\nsc-interop-build\tanda08-flexible-decay"
set "PATH=A:\al82dev-clang\bin;A:\nsc-interop-build\tanda1-ON\bin;%PATH%"
set "NSC_LINKER=A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
if not exist "%DEST%" mkdir "%DEST%"
if errorlevel 1 exit /b 1
set "TMP=%DEST%"
set "TEMP=%DEST%"
for %%F in (Api.h Api.c DecayContexts.ns DecayContexts.nsproj DecayConsumer.ns DecayShared.nsproj DecayMetadata.nsproj Producer.ns Producer.nsproj Restricted.h ApiRestricted.c RestrictedPackedAddress.ns RestrictedPackedAddress.nsproj RestrictedPackedDecay.ns RestrictedPackedDecay.nsproj) do (
  copy /y "%~dp0%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
for %%F in (QualifiedDecay.ns QualifiedDecay.nsproj DecayDiscardConst.ns DecayDiscardConst.nsproj DecayDiscardVolatile.ns DecayDiscardVolatile.nsproj) do (
  copy /y "%~dp0%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 "%DEST%\Api.c" -o "%DEST%\api.obj"
if errorlevel 1 exit /b 1
llvm-lib.exe /nologo /out:"%DEST%\api.lib" "%DEST%\api.obj"
if errorlevel 1 exit /b 1
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 "%DEST%\ApiRestricted.c" -o "%DEST%\restricted.obj"
if errorlevel 1 exit /b 1
llvm-lib.exe /nologo /out:"%DEST%\restricted.lib" "%DEST%\restricted.obj"
if errorlevel 1 exit /b 1
for %%O in (0 1 2 3) do (
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\DecayContexts.nsproj" -O%%O --no-artifact-cache -o "%DEST%\decay-O%%O.exe"
  if errorlevel 1 exit /b 1
  "%DEST%\decay-O%%O.exe"
  if errorlevel 1 exit /b 1
  echo Conversion flexible: O%%O paso.
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-lib "%DEST%\Producer.nsproj" -O%%O --no-artifact-cache -o "%DEST%\CFlexibleProducer.lib"
  if errorlevel 1 exit /b 1
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\DecayShared.nsproj" -O%%O --no-artifact-cache -o "%DEST%\shared-O%%O.exe"
  if errorlevel 1 exit /b 1
  "%DEST%\shared-O%%O.exe"
  if errorlevel 1 exit /b 1
  move /y "%DEST%\Api.h" "%DEST%\Api.hidden" >nul
  if errorlevel 1 exit /b 1
  call :metadata %%O
  set "RESULT=!ERRORLEVEL!"
  move /y "%DEST%\Api.hidden" "%DEST%\Api.h" >nul
  if errorlevel 1 exit /b 1
  if not "!RESULT!"=="0" exit /b !RESULT!
  echo Conversion flexible precompilada: O%%O paso.
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\QualifiedDecay.nsproj" -O%%O --no-artifact-cache -o "%DEST%\qualified-O%%O.exe"
  if errorlevel 1 exit /b 1
  "%DEST%\qualified-O%%O.exe"
  if errorlevel 1 exit /b 1
  echo Conversion flexible calificada: O%%O paso.
)
nsc.exe --cpp-clang-root=A:\al82dev-clang --check-project-method-bodies "%DEST%\RestrictedPackedAddress.nsproj" > "%DEST%\packed.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /l /c:"operand of & must name addressable storage" "%DEST%\packed.log" >nul
if errorlevel 1 exit /b 1
echo Conversion flexible: rechazo subalineado paso.
nsc.exe --cpp-clang-root=A:\al82dev-clang --check-project-method-bodies "%DEST%\RestrictedPackedDecay.nsproj" > "%DEST%\packed-decay.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /l /c:"cannot initialize local" "%DEST%\packed-decay.log" >nul
if errorlevel 1 exit /b 1
echo Conversion flexible implicita subalineada: rechazo confirmado.
for %%P in (DecayDiscardConst DecayDiscardVolatile) do (
  nsc.exe --cpp-clang-root=A:\al82dev-clang --check-project-method-bodies "%DEST%\%%P.nsproj" > "%DEST%\%%P.log" 2>&1
  if not errorlevel 1 exit /b 1
  findstr /l /c:"discard" "%DEST%\%%P.log" >nul
  if errorlevel 1 exit /b 1
  echo Perdida de calificador %%P: rechazo confirmado.
)
exit /b 0

:metadata
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\DecayMetadata.nsproj" -O%1 --no-artifact-cache -o "%DEST%\metadata-O%1.exe"
if errorlevel 1 exit /b 1
"%DEST%\metadata-O%1.exe"
exit /b %ERRORLEVEL%
