@echo off
setlocal
set "DEST=A:\nsc-interop-build\tanda08-tls-directo\scalars"
set "PATH=A:\al82dev-clang\bin;%PATH%"
set "TMP=%DEST%"
set "TEMP=%DEST%"
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 "%~dp0ScalarApi.c" -o "%DEST%\api.obj" > "%DEST%\crt-api-compile.log" 2>&1
if errorlevel 1 exit /b 1
lld-link.exe /nologo /dll /nodefaultlib /entry:_DllMainCRTStartup /include:_tls_used /export:tls_zero,DATA /export:tls_volatile,DATA /export:tls_const,DATA /export:tls_unsigned,DATA /export:tls_double,DATA /export:tls_bool,DATA /out:"%DEST%\api.dll" /implib:"%DEST%\api.lib" "%DEST%\api.obj" "A:\vc2026-runtime\lib\x64\release\libcmt.lib" "A:\vc2026-runtime\lib\x64\release\libcpmt.lib" "A:\vc2026-runtime\lib\x64\release\libvcruntime.lib" "A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\ucrt\x64\libucrt.lib" "A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\um\x64\kernel32.lib" > "%DEST%\crt-api-link.log" 2>&1
if errorlevel 1 (echo Enlace DLL con CRT fallo. & exit /b 1)
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 "%~dp0ScalarControl.c" -o "%DEST%\crt-control.obj" > "%DEST%\crt-control-compile.log" 2>&1
if errorlevel 1 exit /b 1
lld-link.exe /nologo /nodefaultlib /subsystem:console /entry:mainCRTStartup /out:"%DEST%\crt-control.exe" "%DEST%\crt-control.obj" "%DEST%\api.lib" "A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\um\x64\kernel32.lib" > "%DEST%\crt-control-link.log" 2>&1
if errorlevel 1 exit /b 1
"%DEST%\crt-control.exe"
if errorlevel 1 (echo Control TLS con CRT fallo. & exit /b 1)
echo Control C TLS con CRT estatico correcto.
set "PATH=A:\nsc-interop-build\tanda1-ON\bin;%PATH%"
set "NSC_LINKER=A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
for %%F in (ScalarApi.h ScalarNative.ns ScalarProject.nsproj) do (
  copy /y "%~dp0%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
for %%O in (0 1 2 3) do (
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\ScalarProject.nsproj" -O%%O --no-artifact-cache -o "%DEST%\crt-scalar-O%%O.exe" > "%DEST%\crt-nsc-O%%O.log" 2>&1
  if errorlevel 1 (echo Compilacion TLS DLL con CRT O%%O fallo. & exit /b 1)
  "%DEST%\crt-scalar-O%%O.exe"
  if errorlevel 1 (echo Ejecucion TLS DLL con CRT O%%O fallo. & exit /b 1)
  echo Escalares TLS DLL con CRT O%%O correctos.
)
exit /b 0
