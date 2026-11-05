@echo off
setlocal
set "TOOLS=A:\al82dev-clang\bin"
set "OUT=A:\nsc-interop-build\cpp-fixtures\exception-control"
set "CRT=A:\vc2026-runtime\lib\x64\release"
set "UCRT=A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\ucrt\x64"
if not exist "%OUT%" mkdir "%OUT%"
"%TOOLS%\clang.exe" --target=x86_64-pc-windows-msvc -std=c++20 -O0 -c "%~dp0cpp-exception-control.cpp" -o "%OUT%\control.obj"
if errorlevel 1 exit /b 1
"%TOOLS%\lld-link.exe" /nologo /entry:mainCRTStartup /subsystem:console /nodefaultlib /out:"%OUT%\control.exe" "%OUT%\control.obj" "%CRT%\libcmt.lib" "%CRT%\libvcruntime.lib" "%UCRT%\libucrt.lib" "A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\um\x64\kernel32.Lib"
if errorlevel 1 exit /b 1
"%OUT%\control.exe"
if errorlevel 1 exit /b 1
echo Control C++ de excepciones: resultado correcto.
exit /b 0
