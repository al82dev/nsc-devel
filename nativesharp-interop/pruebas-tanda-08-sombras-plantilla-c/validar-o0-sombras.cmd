@echo off
setlocal
set "DEST=A:\nsc-interop-build\t08-shadow-o0"
set "SRC=%~dp0"
set "PATH=A:\nsc-interop-build\tanda1-ON\bin;A:\al82dev-clang\bin;%PATH%"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
set "NSC_LINKER=A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
if not exist "%DEST%" mkdir "%DEST%"
for %%F in (Api.h Producer.ns Producer.nsproj Consumer.ns Consumer.nsproj) do (
  copy /y "%SRC%\%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-lib "%DEST%\Producer.nsproj" -O0 --no-artifact-cache -o "%DEST%\ShadowProducer.lib" > "%DEST%\Producer.log" 2>&1
if errorlevel 1 (type "%DEST%\Producer.log" & exit /b 1)
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Consumer.nsproj" -O0 --no-artifact-cache -o "%DEST%\Consumer.exe" > "%DEST%\Consumer.log" 2>&1
if errorlevel 1 (type "%DEST%\Consumer.log" & exit /b 1)
"%DEST%\Consumer.exe"
if errorlevel 1 exit /b 1
echo Sombras foreach y catch: ejecucion O0 correcta.
exit /b 0
