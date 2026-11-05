@echo off
setlocal
set "TOOLS=A:\al82dev-clang\bin"
set "OUT=A:\nsc-interop-build\cpp-fixtures\exception-cross"
set "CRT=A:\vc2026-runtime\lib\x64\release"
set "UCRT=A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\ucrt\x64"
if not exist "%OUT%" mkdir "%OUT%"
copy /y "%~dp0cpp-exception-cross.h" "%OUT%\cpp-exception-cross.h" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0cpp-exception-cross.ns.txt" "%OUT%\Main.ns" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0cpp-exception-cross.nsproj" "%OUT%\CppProject.nsproj" >nul
if errorlevel 1 exit /b 1
"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-obj "%OUT%\CppProject.nsproj" --cpp-clang-root=A:\al82dev-clang -o "%OUT%\ns.obj"
if errorlevel 1 exit /b 1
"%TOOLS%\clang.exe" --target=x86_64-pc-windows-msvc -std=c++20 -O0 -c "%~dp0cpp-exception-cross.cpp" -o "%OUT%\cross.obj"
if errorlevel 1 exit /b 1
"%TOOLS%\lld-link.exe" /nologo /entry:mainCRTStartup /subsystem:console /nodefaultlib /out:"%OUT%\cross.exe" "%OUT%\cross.obj" "%OUT%\ns.obj" "A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib" "%CRT%\libcmt.lib" "%CRT%\libvcruntime.lib" "%UCRT%\libucrt.lib" "A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\um\x64\kernel32.Lib" "A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\um\x64\Shell32.Lib"
if errorlevel 1 exit /b 1
"%OUT%\cross.exe"
if errorlevel 1 exit /b 1
echo Excepcion C++ cruza finally de NativeSharp: resultado correcto.
copy /y "%~dp0cpp-exception-raii.ns.txt" "%OUT%\Main.ns" >nul
if errorlevel 1 exit /b 1
"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-obj "%OUT%\CppProject.nsproj" --cpp-clang-root=A:\al82dev-clang -o "%OUT%\ns-raii.obj"
if errorlevel 1 exit /b 1
"%TOOLS%\lld-link.exe" /nologo /entry:mainCRTStartup /subsystem:console /nodefaultlib /out:"%OUT%\raii.exe" "%OUT%\cross.obj" "%OUT%\ns-raii.obj" "A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib" "%CRT%\libcmt.lib" "%CRT%\libvcruntime.lib" "%UCRT%\libucrt.lib" "A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\um\x64\kernel32.Lib" "A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\um\x64\Shell32.Lib"
if errorlevel 1 exit /b 1
"%OUT%\raii.exe"
if errorlevel 1 exit /b 1
echo Excepcion C++ destruye objeto raii NativeSharp: resultado correcto.
"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-obj "%OUT%\CppProject.nsproj" -O2 --cpp-clang-root=A:\al82dev-clang -o "%OUT%\ns-raii-o2.obj"
if errorlevel 1 exit /b 1
"%TOOLS%\lld-link.exe" /nologo /entry:mainCRTStartup /subsystem:console /nodefaultlib /out:"%OUT%\raii-o2.exe" "%OUT%\cross.obj" "%OUT%\ns-raii-o2.obj" "A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib" "%CRT%\libcmt.lib" "%CRT%\libvcruntime.lib" "%UCRT%\libucrt.lib" "A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\um\x64\kernel32.Lib" "A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\um\x64\Shell32.Lib"
if errorlevel 1 exit /b 1
"%OUT%\raii-o2.exe"
if errorlevel 1 exit /b 1
echo Excepcion C++ destruye objeto raii NativeSharp O2: resultado correcto.
copy /y "%~dp0cpp-exception-cross.ns.txt" "%OUT%\Main.ns" >nul
if errorlevel 1 exit /b 1
"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-obj "%OUT%\CppProject.nsproj" -O2 --cpp-clang-root=A:\al82dev-clang -o "%OUT%\ns-finally-o2.obj"
if errorlevel 1 exit /b 1
"%TOOLS%\lld-link.exe" /nologo /entry:mainCRTStartup /subsystem:console /nodefaultlib /out:"%OUT%\finally-o2.exe" "%OUT%\cross.obj" "%OUT%\ns-finally-o2.obj" "A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib" "%CRT%\libcmt.lib" "%CRT%\libvcruntime.lib" "%UCRT%\libucrt.lib" "A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\um\x64\kernel32.Lib" "A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\um\x64\Shell32.Lib"
if errorlevel 1 exit /b 1
"%OUT%\finally-o2.exe"
if errorlevel 1 exit /b 1
echo Excepcion C++ cruza finally de NativeSharp O2: resultado correcto.
exit /b 0
