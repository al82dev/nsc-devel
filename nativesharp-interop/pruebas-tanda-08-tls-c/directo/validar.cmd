@echo off
setlocal
set "DEST=A:\nsc-interop-build\tanda08-tls-directo"
set "PATH=A:\al82dev-clang\bin;A:\nsc-interop-build\tanda1-ON\bin;%PATH%"
if not exist "%DEST%" mkdir "%DEST%"
if errorlevel 1 exit /b 1
set "TMP=%DEST%"
set "TEMP=%DEST%"
set "NSC_LINKER=A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
for %%F in (TlsApi.h TlsApi.c Native.ns Project.nsproj Probe.c) do (
  copy /y "%~dp0%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 "%DEST%\TlsApi.c" -o "%DEST%\api.obj" > "%DEST%\dll-compile.log" 2>&1
if errorlevel 1 (echo Compilacion DLL fallo. & exit /b 1)
lld-link.exe /nologo /dll /noentry /nodefaultlib /include:_tls_used /out:"%DEST%\api.dll" /implib:"%DEST%\api.lib" "%DEST%\api.obj" "A:\vc2026-runtime\lib\x64\release\libcmt.lib" "A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\um\x64\kernel32.lib" > "%DEST%\dll-link.log" 2>&1
if errorlevel 1 (echo Enlace DLL fallo. & exit /b 1)
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 "%DEST%\Probe.c" -o "%DEST%\probe.obj" > "%DEST%\probe-compile.log" 2>&1
if errorlevel 1 (echo Compilacion consumidor C fallo. & exit /b 1)
llvm-readobj.exe --relocations "%DEST%\probe.obj" > "%DEST%\probe-relocations.txt" 2>&1
if errorlevel 1 exit /b 1
lld-link.exe /nologo /nodefaultlib /subsystem:console /entry:mainCRTStartup /include:_tls_used /out:"%DEST%\probe.exe" "%DEST%\probe.obj" "%DEST%\api.lib" "A:\vc2026-runtime\lib\x64\release\libcmt.lib" "A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\um\x64\kernel32.lib" > "%DEST%\probe-link.log" 2>&1
if errorlevel 1 (echo Enlace consumidor C fallo. & exit /b 1)
"%DEST%\probe.exe"
if errorlevel 1 (echo Consumidor C TLS fallo. & exit /b 1)
echo Consumidor C TLS correcto.
for %%O in (0 1 2 3) do (
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O%%O --no-artifact-cache -o "%DEST%\native-O%%O.exe" > "%DEST%\native-O%%O.log" 2>&1
  if errorlevel 1 (echo Compilacion NS TLS O%%O fallo. & exit /b 1)
  "%DEST%\native-O%%O.exe"
  if errorlevel 1 (echo Ejecucion NS TLS O%%O fallo. & exit /b 1)
  echo Acceso TLS directo O%%O correcto.
)
echo TLS DIRECTO COMPLETO
exit /b 0
