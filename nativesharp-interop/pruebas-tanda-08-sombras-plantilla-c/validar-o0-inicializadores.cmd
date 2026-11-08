@echo off
setlocal
set "DEST=A:\nsc-interop-build\t08-initializer-o0"
set "SRC=%~dp0"
set "PATH=A:\nsc-interop-build\tanda1-ON\bin;A:\al82dev-clang\bin;%PATH%"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
set "NSC_LINKER=A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
if not exist "%DEST%" mkdir "%DEST%"
for %%F in (Api.h ScopeProducer.ns ScopeProducer.nsproj ScopeConsumer.ns ScopeConsumer.nsproj InitializerProducer.ns InitializerProducer.nsproj InitializerConsumer.ns InitializerConsumer.nsproj) do (
  copy /y "%SRC%\%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
for %%P in (Scope Initializer) do (
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-lib "%DEST%\%%PProducer.nsproj" -O0 --no-artifact-cache -o "%DEST%\%%PProducer.lib" > "%DEST%\%%PProducer.log" 2>&1
  if errorlevel 1 exit /b 1
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\%%PConsumer.nsproj" -O0 --no-artifact-cache -o "%DEST%\%%PConsumer.exe" > "%DEST%\%%PConsumer.log" 2>&1
  if errorlevel 1 exit /b 1
  "%DEST%\%%PConsumer.exe"
  if errorlevel 1 exit /b 1
)
echo Inicializadores y ambitos: ejecucion O0 correcta.
exit /b 0
