@echo off
setlocal
set "DEST=A:\nsc-interop-build\tls-abi\crt-control"
if not exist "%DEST%" mkdir "%DEST%"
set "PATH=A:\al82dev-clang\bin;%PATH%"
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 "%~dp0probe_tls_crt_control.c" -o "%DEST%\control.obj"
if errorlevel 1 exit /b 1
lld-link.exe /nologo /nodefaultlib /subsystem:console /map:"%DEST%\control.map" /out:"%DEST%\control.exe" "%DEST%\control.obj" "A:\vc2026-runtime\lib\x64\release\libcmt.lib" "A:\vc2026-runtime\lib\x64\release\libvcruntime.lib" "A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\ucrt\x64\libucrt.lib" "A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\um\x64\kernel32.lib"
if errorlevel 1 exit /b 1
llvm-readobj.exe --file-headers "%DEST%\control.exe" > "%DEST%\headers.txt"
if errorlevel 1 exit /b 1
findstr /L /C:"mainCRTStartup" "%DEST%\control.map" >nul
if errorlevel 1 exit /b 1
copy /y A:\nsc-interop-build\tanda08-tls-directo\scalars\api.dll "%DEST%\api.dll" >nul
if errorlevel 1 exit /b 1
"%DEST%\control.exe"
set "ORIGINAL=%ERRORLEVEL%"
echo Original CRT control: %ORIGINAL%.
copy /y A:\nsc-interop-build\tls-abi\tls_import_empty_inside_nosentinel.dll "%DEST%\api.dll" >nul
if errorlevel 1 exit /b 1
"%DEST%\control.exe"
set "MODIFIED=%ERRORLEVEL%"
echo Experimental CRT control: %MODIFIED%.
if not "%ORIGINAL%"=="8" exit /b 1
if not "%MODIFIED%"=="0" exit /b 1
exit /b 0
