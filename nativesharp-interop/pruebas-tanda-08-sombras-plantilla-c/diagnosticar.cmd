@echo off
setlocal EnableDelayedExpansion
set "DEST=A:\nsc-interop-build\tanda08-h3-shadow-fixture"
set "PATH=A:\al82dev-clang\bin;A:\nsc-interop-build\tanda1-ON\bin;%PATH%"
set "NSC_LINKER=A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
if not exist "%DEST%" mkdir "%DEST%"
if errorlevel 1 exit /b 2
set "TEMP=%DEST%"
set "TMP=%DEST%"
for %%F in (Api.h Producer.ns Producer.nsproj Consumer.ns Consumer.nsproj) do (
  copy /y "%~dp0%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 2
)
set "RESULT=0"
for %%O in (0 1 2 3) do (
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-lib "%DEST%\Producer.nsproj" -O%%O --no-artifact-cache -o "%DEST%\ShadowProducer.lib" > "%DEST%\producer-O%%O.log" 2>&1
  if errorlevel 1 exit /b 2
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Consumer.nsproj" -O%%O --no-artifact-cache -o "%DEST%\consumer-O%%O.exe" > "%DEST%\consumer-O%%O.log" 2>&1
  if errorlevel 1 exit /b 2
  "%DEST%\consumer-O%%O.exe"
  if errorlevel 1 exit /b 2
  nsc.exe --verify-library-metadata "%DEST%\ShadowProducer.nsmeta" > "%DEST%\metadata-O%%O.log" 2>&1
  if errorlevel 1 exit /b 2
  nsc_cpp_c_identity_tests.exe --inspect-template-global "%DEST%\ShadowProducer.nsmeta" shadow_value > "%DEST%\global-O%%O.log" 2>&1
  if not errorlevel 1 (
    echo O%%O: global C sombreada capturada indebidamente.
    set "RESULT=1"
  ) else (
    findstr /l /c:"FAIL: real NSMETA carries the requested C template global" "%DEST%\global-O%%O.log" >nul
    if errorlevel 1 exit /b 2
  )
  llvm-nm.exe "%DEST%\ShadowProducer.lib" > "%DEST%\symbols-O%%O.log"
  if errorlevel 1 exit /b 2
  findstr /l /c:"__nsc_c_inline_" "%DEST%\symbols-O%%O.log" >nul
  if not errorlevel 1 (
    echo O%%O: funcion inline C sombreada emitida indebidamente.
    set "RESULT=1"
  )
)
exit /b %RESULT%
