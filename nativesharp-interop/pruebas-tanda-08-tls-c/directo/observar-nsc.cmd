@echo off
setlocal
set "DEST=A:\nsc-interop-build\tanda08-tls-directo"
set "PATH=A:\al82dev-clang\bin;A:\nsc-interop-build\tanda1-ON\bin;%PATH%"
set "TMP=%DEST%"
set "TEMP=%DEST%"
set "NSC_LINKER=A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
for %%F in (TlsApi.h Native.ns Project.nsproj) do copy /y "%~dp0%%F" "%DEST%\%%F" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O0 --no-artifact-cache -o "%DEST%\observed-native.exe" > "%DEST%\observed-native.log" 2>&1
if errorlevel 1 (
  echo NSC no produjo ejecutable; ver observed-native.log.
  exit /b 0
)
"%DEST%\observed-native.exe"
if errorlevel 1 (
  echo NSC produjo ejecutable pero la ejecucion fallo.
  exit /b 0
)
echo NSC produjo y ejecuto el caso TLS directo.
exit /b 0
