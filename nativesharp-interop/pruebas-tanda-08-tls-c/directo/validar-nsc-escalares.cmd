@echo off
setlocal
set "DEST=A:\nsc-interop-build\tanda08-tls-directo\scalars"
set "PATH=A:\al82dev-clang\bin;A:\nsc-interop-build\tanda1-ON\bin;%PATH%"
set "NSC_LINKER=A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
if not exist "%DEST%" mkdir "%DEST%"
set "TMP=%DEST%"
set "TEMP=%DEST%"
for %%F in (ScalarApi.h ScalarApi.c ScalarControl.c ScalarNative.ns ScalarProject.nsproj) do (
  copy /y "%~dp0%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 "%DEST%\ScalarApi.c" -o "%DEST%\api.obj" > "%DEST%\compile.log" 2>&1
if errorlevel 1 exit /b 1
lld-link.exe /nologo /dll /noentry /nodefaultlib /include:_tls_used /export:tls_zero,DATA /export:tls_volatile,DATA /export:tls_const,DATA /export:tls_unsigned,DATA /export:tls_double,DATA /export:tls_bool,DATA /out:"%DEST%\api.dll" /implib:"%DEST%\api.lib" "%DEST%\api.obj" "A:\vc2026-runtime\lib\x64\release\libcmt.lib" "A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\um\x64\kernel32.lib" > "%DEST%\dll-link.log" 2>&1
if errorlevel 1 exit /b 1
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 "%DEST%\ScalarControl.c" -o "%DEST%\control.obj" > "%DEST%\control-compile.log" 2>&1
if errorlevel 1 exit /b 1
lld-link.exe /nologo /nodefaultlib /subsystem:console /entry:mainCRTStartup /out:"%DEST%\control.exe" "%DEST%\control.obj" "%DEST%\api.lib" "A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\um\x64\kernel32.lib" > "%DEST%\control-link.log" 2>&1
if errorlevel 1 exit /b 1
"%DEST%\control.exe"
if errorlevel 1 (echo Control C TLS cero fallo. & exit /b 1)
echo Control C TLS cero correcto.
for %%O in (0 1 2 3) do (
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\ScalarProject.nsproj" -O%%O --no-artifact-cache -o "%DEST%\scalar-O%%O.exe" > "%DEST%\nsc-O%%O.log" 2>&1
  if errorlevel 1 (echo Compilacion TLS escalares DLL O%%O fallo. & exit /b 1)
  "%DEST%\scalar-O%%O.exe"
  if errorlevel 1 (echo Ejecucion TLS escalares DLL O%%O fallo. & exit /b 1)
  echo Escalares TLS de DLL O%%O correctos.
)
exit /b 0
