@echo off
setlocal
set "DEST=A:\nsc-interop-build\cpp-fixtures\command-cpp-class-dll"
set "TOOLS=A:\al82dev-clang\bin"
set "OPT=-O0"
if "%1"=="O2" set "OPT=-O2"
if not exist "%DEST%" mkdir "%DEST%"
copy /y "%~dp0CppDllClass.h" "%DEST%\CppDllClass.h" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0CppDllClass.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0CppDllClass.nsproj" "%DEST%\CppProject.nsproj" >nul
if errorlevel 1 exit /b 1
copy /y "C:\native-sharp\bin\nslink.exe" "%DEST%\nslink.exe" >nul
if errorlevel 1 exit /b 1
set "NSC_LINKER=%DEST%\nslink.exe"
set "NSC_LLVM_LIB=%TOOLS%\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
"%TOOLS%\clang.exe" --target=x86_64-pc-windows-msvc -c "%~dp0..\pruebas-llamadas-escalares\cpp-dll-field-library.cpp" -o "%DEST%\library.obj"
if errorlevel 1 exit /b 1
"%TOOLS%\lld-link.exe" /nologo /dll /noentry /nodefaultlib /out:"%DEST%\cpp-dll-field-library.dll" /implib:"%DEST%\cpp-dll-field-library.lib" "%DEST%\library.obj"
if errorlevel 1 exit /b 1
"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-exe "%DEST%\CppProject.nsproj" %OPT% --no-artifact-cache --cpp-clang-root=A:\al82dev-clang -o "%DEST%\CppProject.exe"
if errorlevel 1 exit /b 1
"%DEST%\CppProject.exe"
if not errorlevel 21 exit /b 1
if errorlevel 22 exit /b 1
echo Proyecto NativeSharp con clase C++ en DLL: resultado 21.
exit /b 0
