@echo off
setlocal EnableDelayedExpansion
set "DEST=A:\nsc-interop-build\tanda08-h3-shadow-scope"
set "PATH=A:\al82dev-clang\bin;A:\nsc-interop-build\tanda1-ON\bin;%PATH%"
set "NSC_LINKER=A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
if not exist "%DEST%" mkdir "%DEST%"
if errorlevel 1 exit /b 2
set "TEMP=%DEST%"
set "TMP=%DEST%"
for %%F in (Api.h ScopeProducer.ns ScopeProducer.nsproj ScopeConsumer.ns ScopeConsumer.nsproj) do (
  copy /y "%~dp0%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 2
)
for %%O in (0 1 2 3) do (
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-lib "%DEST%\ScopeProducer.nsproj" -O%%O --no-artifact-cache -o "%DEST%\ScopeProducer.lib" > "%DEST%\producer-O%%O.log" 2>&1
  if errorlevel 1 exit /b 2
  nsc.exe --verify-library-metadata "%DEST%\ScopeProducer.nsmeta" > "%DEST%\metadata-O%%O.log" 2>&1
  if errorlevel 1 exit /b 2
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\ScopeConsumer.nsproj" -O%%O --no-artifact-cache -o "%DEST%\consumer-O%%O.exe" > "%DEST%\consumer-O%%O.log" 2>&1
  if errorlevel 1 exit /b 2
  "%DEST%\consumer-O%%O.exe"
  if errorlevel 1 exit /b 1
  echo O%%O: dependencia C fuera del bloque sombreado correcta.
)
exit /b 0
