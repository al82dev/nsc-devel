@echo off
setlocal
set "DEST=A:\nsc-interop-build\tanda08-tls-directo"
set "PATH=A:\al82dev-clang\bin;%PATH%"
if not exist "%DEST%" mkdir "%DEST%"
set "TMP=%DEST%"
set "TEMP=%DEST%"
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 "%~dp0TlsApi.c" -o "%DEST%\pe-api.obj" > "%DEST%\pe-compile.log" 2>&1
if errorlevel 1 exit /b 1
lld-link.exe /nologo /dll /noentry /nodefaultlib /include:_tls_used /export:tls_exported,DATA /out:"%DEST%\api.dll" /implib:"%DEST%\api.lib" "%DEST%\pe-api.obj" "A:\vc2026-runtime\lib\x64\release\libcmt.lib" "A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\um\x64\kernel32.lib" > "%DEST%\pe-dll-link.log" 2>&1
if errorlevel 1 exit /b 1
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 "%~dp0ProbePeTls.c" -o "%DEST%\pe-probe.obj" > "%DEST%\pe-probe-compile.log" 2>&1
if errorlevel 1 exit /b 1
lld-link.exe /nologo /nodefaultlib /subsystem:console /entry:mainCRTStartup /out:"%DEST%\pe-probe.exe" "%DEST%\pe-probe.obj" "%DEST%\api.lib" "A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\um\x64\kernel32.lib" > "%DEST%\pe-probe-link.log" 2>&1
if errorlevel 1 exit /b 1
"%DEST%\pe-probe.exe"
if errorlevel 1 exit /b 1
echo Experimento PE TLS directo: lectura y escritura en dos hilos correctas.
exit /b 0
