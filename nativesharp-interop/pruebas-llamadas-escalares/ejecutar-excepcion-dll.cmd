@echo off
setlocal
set "TOOLS=A:\al82dev-clang\bin"
set "OUT=A:\nsc-interop-build\cpp-fixtures\exception-dll"
set "CRT=A:\vc2026-runtime\lib\x64\release"
set "UCRT=A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\ucrt\x64"
set "SDK=A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\um\x64"
set "TYPEFLAG="
if "%1"=="tipo" set "TYPEFLAG=-DCPP_EXCEPTION_OBJECT"
if "%1"=="tipo" set "OUT=A:\nsc-interop-build\cpp-fixtures\exception-dll-type"
if "%1"=="no-trivial" set "TYPEFLAG=-DCPP_EXCEPTION_OBJECT -DCPP_EXCEPTION_NONTRIVIAL"
if "%1"=="no-trivial" set "OUT=A:\nsc-interop-build\cpp-fixtures\exception-dll-nontrivial"
if not exist "%OUT%" mkdir "%OUT%"
copy /y "%~dp0cpp-exception-dll.h" "%OUT%\cpp-exception-cross.h" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0cpp-exception-cross.nsproj" "%OUT%\CppProject.nsproj" >nul
if errorlevel 1 exit /b 1
"%TOOLS%\clang.exe" --target=x86_64-pc-windows-msvc -std=c++20 -O0 %TYPEFLAG% -c "%~dp0cpp-exception-dll.cpp" -o "%OUT%\dll.obj"
if errorlevel 1 exit /b 1
"%TOOLS%\lld-link.exe" /nologo /dll /entry:_DllMainCRTStartup /nodefaultlib /out:"%OUT%\cpp-exception-dll.dll" /implib:"%OUT%\cpp-exception-dll.lib" "%OUT%\dll.obj" "%CRT%\libcmt.lib" "%CRT%\libvcruntime.lib" "%UCRT%\libucrt.lib" "%SDK%\kernel32.Lib"
if errorlevel 1 exit /b 1
"%TOOLS%\clang.exe" --target=x86_64-pc-windows-msvc -std=c++20 -O0 -DCPP_THROW_DLL %TYPEFLAG% -c "%~dp0cpp-exception-cross.cpp" -o "%OUT%\cross.obj"
if errorlevel 1 exit /b 1
for %%S in (cross raii) do for %%O in (0 2) do (
    call :run %%S %%O
    if errorlevel 1 exit /b 1
)
call :run no-cleanup 0
if not errorlevel 2 exit /b 1
if errorlevel 3 exit /b 1
echo Control negativo: ausencia de limpieza detectada.
echo Excepciones desde DLL: escenarios finally y raii correctos en O0 y O2.
exit /b 0
:run
if "%1"=="cross" copy /y "%~dp0cpp-exception-cross.ns.txt" "%OUT%\Main.ns" >nul
if "%1"=="raii" copy /y "%~dp0cpp-exception-raii.ns.txt" "%OUT%\Main.ns" >nul
if "%1"=="no-cleanup" copy /y "%~dp0cpp-exception-no-cleanup.ns.txt" "%OUT%\Main.ns" >nul
if errorlevel 1 exit /b 1
"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-obj "%OUT%\CppProject.nsproj" -O%2 --cpp-clang-root=A:\al82dev-clang -o "%OUT%\ns-%1-%2.obj"
if errorlevel 1 exit /b 1
"%TOOLS%\lld-link.exe" /nologo /entry:mainCRTStartup /subsystem:console /nodefaultlib /out:"%OUT%\%1-%2.exe" "%OUT%\cross.obj" "%OUT%\ns-%1-%2.obj" "%OUT%\cpp-exception-dll.lib" "A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib" "%CRT%\libcmt.lib" "%CRT%\libvcruntime.lib" "%UCRT%\libucrt.lib" "%SDK%\kernel32.Lib" "%SDK%\Shell32.Lib"
if errorlevel 1 exit /b 1
"%OUT%\%1-%2.exe"
exit /b %errorlevel%
