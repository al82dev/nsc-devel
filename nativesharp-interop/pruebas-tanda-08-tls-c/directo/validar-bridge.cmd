@echo off
setlocal
set "DEST=A:\nsc-interop-build\tanda08-tls-directo\bridge"
set "PATH=A:\al82dev-clang\bin;A:\nsc-interop-build\tanda1-ON\bin;%PATH%"
set "NSC_LINKER=A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
if not exist "%DEST%" mkdir "%DEST%"
set "TMP=%DEST%"
set "TEMP=%DEST%"
for %%F in (TlsApi.c Bridge.c BridgeApi.h BridgeClient.h Bridge.ns Bridge.nsproj) do (
  copy /y "%~dp0%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 "%DEST%\TlsApi.c" -o "%DEST%\api.obj" > "%DEST%\api-compile.log" 2>&1
if errorlevel 1 exit /b 1
lld-link.exe /nologo /dll /noentry /nodefaultlib /include:_tls_used /export:tls_exported,DATA /out:"%DEST%\api.dll" /implib:"%DEST%\api.lib" "%DEST%\api.obj" "A:\vc2026-runtime\lib\x64\release\libcmt.lib" "A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\um\x64\kernel32.lib" > "%DEST%\api-link.log" 2>&1
if errorlevel 1 exit /b 1
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 "%DEST%\Bridge.c" -o "%DEST%\bridge.obj" > "%DEST%\bridge-compile.log" 2>&1
if errorlevel 1 exit /b 1
llvm-lib.exe /nologo /out:"%DEST%\bridge.lib" "%DEST%\bridge.obj" > "%DEST%\bridge-lib.log" 2>&1
if errorlevel 1 exit /b 1
for %%O in (0 1 2 3) do (
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Bridge.nsproj" -O%%O --no-artifact-cache -o "%DEST%\bridge-O%%O.exe" > "%DEST%\nsc-O%%O.log" 2>&1
  if errorlevel 1 (echo Enlace bridge O%%O fallo. & exit /b 1)
  "%DEST%\bridge-O%%O.exe"
  if errorlevel 1 (echo Ejecucion bridge O%%O fallo. & exit /b 1)
  echo Instrumento TLS bridge O%%O correcto.
)
exit /b 0
