@echo off
setlocal
set "DEST=A:\nsc-interop-build\tanda08-tls-directo"
set "PATH=A:\al82dev-clang\bin;A:\nsc-interop-build\tanda1-ON\bin;%PATH%"
set "NSC_LINKER=A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
if not exist "%DEST%" mkdir "%DEST%"
set "TMP=%DEST%"
set "TEMP=%DEST%"
call "%~dp0validar-pe-tls.cmd"
if errorlevel 1 exit /b 1
for %%F in (TlsApi.h Native.ns Project.nsproj) do (
  copy /y "%~dp0%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
for %%O in (0 1 2 3) do (
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O%%O --no-artifact-cache -o "%DEST%\direct-nsc-O%%O.exe" > "%DEST%\direct-nsc-O%%O.log" 2>&1
  if errorlevel 1 (echo Compilacion directa O%%O fallo. & exit /b 1)
  "%DEST%\direct-nsc-O%%O.exe"
  if errorlevel 1 (echo Acceso directo O%%O fallo. & exit /b 1)
  echo Acceso directo NSC TLS DLL O%%O correcto.
)
exit /b 0
