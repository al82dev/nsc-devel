@echo off
setlocal
set "TOOLS=A:\al82dev-clang\bin"
set "OUT=A:\nsc-interop-build\cpp-fixtures\exception-methods"
set "CRT=A:\vc2026-runtime\lib\x64\release"
set "UCRT=A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\ucrt\x64"
set "SDK=A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\um\x64"
if not exist "%OUT%" mkdir "%OUT%"
copy /y "%~dp0cpp-throw-method.h" "%OUT%\cpp-throw-method.h" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0cpp-throw-method.nsproj" "%OUT%\CppProject.nsproj" >nul
if errorlevel 1 exit /b 1
"%TOOLS%\clang.exe" --target=x86_64-pc-windows-msvc -std=c++20 -O0 -c "%~dp0cpp-throw-method.cpp" -o "%OUT%\methods.obj"
if errorlevel 1 exit /b 1
"%TOOLS%\clang.exe" --target=x86_64-pc-windows-msvc -std=c++20 -O0 -c "%~dp0cpp-exception-cross.cpp" -o "%OUT%\cross.obj"
if errorlevel 1 exit /b 1
for %%S in (instance static) do for %%O in (0 2) do (
    call :run %%S %%O
    if errorlevel 1 exit /b 1
)
echo Metodos C++ lanzadores y limpieza raii NS correctos en O0 y O2.
exit /b 0
:run
copy /y "%~dp0cpp-throw-method-%1.ns.txt" "%OUT%\Main.ns" >nul
if errorlevel 1 exit /b 1
"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-obj "%OUT%\CppProject.nsproj" -O%2 --cpp-clang-root=A:\al82dev-clang -o "%OUT%\ns-%1-%2.obj"
if errorlevel 1 exit /b 1
"%TOOLS%\lld-link.exe" /nologo /entry:mainCRTStartup /subsystem:console /nodefaultlib /out:"%OUT%\%1-%2.exe" "%OUT%\cross.obj" "%OUT%\methods.obj" "%OUT%\ns-%1-%2.obj" "A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib" "%CRT%\libcmt.lib" "%CRT%\libvcruntime.lib" "%UCRT%\libucrt.lib" "%SDK%\kernel32.Lib" "%SDK%\Shell32.Lib"
if errorlevel 1 exit /b 1
"%OUT%\%1-%2.exe"
exit /b %errorlevel%
