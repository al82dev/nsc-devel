@echo off
setlocal
set "DEST=A:\nsc-interop-build\tanda08-tls-directo\scalars"
set "PATH=A:\al82dev-clang\bin;%PATH%"
set "TMP=%DEST%"
set "TEMP=%DEST%"
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 -DTLS_PROBE_IMPORT "%~dp0ScalarApi.c" -o "%DEST%\api.obj" > "%DEST%\dynamic-api-compile.log" 2>&1
if errorlevel 1 exit /b 1
lld-link.exe /nologo /dll /noentry /nodefaultlib /include:_tls_used /export:tls_zero,DATA /out:"%DEST%\api.dll" "%DEST%\api.obj" "A:\vc2026-runtime\lib\x64\release\libcmt.lib" "A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\um\x64\kernel32.lib" > "%DEST%\dynamic-api-link.log" 2>&1
if errorlevel 1 exit /b 1
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 "%~dp0ScalarLoadProbe.c" -o "%DEST%\dynamic-probe.obj" > "%DEST%\dynamic-probe-compile.log" 2>&1
if errorlevel 1 exit /b 1
lld-link.exe /nologo /nodefaultlib /subsystem:console /entry:mainCRTStartup /out:"%DEST%\dynamic-probe.exe" "%DEST%\dynamic-probe.obj" "A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\um\x64\kernel32.lib" > "%DEST%\dynamic-probe-link.log" 2>&1
if errorlevel 1 exit /b 1
"%DEST%\dynamic-probe.exe"
if errorlevel 1 (echo Control C TLS por carga dinamica fallo. & exit /b 1)
echo Control C TLS por carga dinamica correcto.
exit /b 0
