@echo off
setlocal
set "DEST=A:\nsc-interop-build\tanda08-tls-directo\native-thread"
set "PATH=A:\al82dev-clang\bin;A:\nsc-interop-build\tanda1-ON\bin;%PATH%"
set "NSC_LINKER=A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
if not exist "%DEST%" mkdir "%DEST%"
set "TMP=%DEST%"
set "TEMP=%DEST%"
for %%F in (TlsApi.h TlsApi.c ThreadApi.h ThreadApi.c ThreadNative.ns ThreadProject.nsproj) do (
  copy /y "%~dp0%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
for %%F in (TlsApi ThreadApi) do (
  clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 "%DEST%\%%F.c" -o "%DEST%\%%F.obj" > "%DEST%\%%F.log" 2>&1
  if errorlevel 1 exit /b 1
)
lld-link.exe /nologo /dll /noentry /nodefaultlib /include:_tls_used /export:tls_exported,DATA /out:"%DEST%\api.dll" /implib:"%DEST%\api.lib" "%DEST%\TlsApi.obj" "%DEST%\ThreadApi.obj" "A:\vc2026-runtime\lib\x64\release\libcmt.lib" "A:\vc2026-runtime\lib\x64\release\libcpmt.lib" "A:\vc2026-runtime\lib\x64\release\libvcruntime.lib" "A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\ucrt\x64\libucrt.lib" "A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\um\x64\kernel32.lib" > "%DEST%\dll-link.log" 2>&1
if errorlevel 1 exit /b 1
for %%O in (0 1 2 3) do (
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\ThreadProject.nsproj" -O%%O --no-artifact-cache -o "%DEST%\native-O%%O.exe" > "%DEST%\native-O%%O.log" 2>&1
  if errorlevel 1 (echo Compilacion callback TLS O%%O fallo. & exit /b 1)
  "%DEST%\native-O%%O.exe"
  if errorlevel 1 (echo Ejecucion callback TLS O%%O fallo. & exit /b 1)
  echo Acceso NativeSharp TLS en dos hilos O%%O correcto.
)
exit /b 0
