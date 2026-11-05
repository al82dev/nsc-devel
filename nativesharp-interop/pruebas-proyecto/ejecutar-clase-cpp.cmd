@echo off
setlocal
set "DEST=A:\nsc-interop-build\cpp-fixtures\command-cpp-class"
set "TOOLS=A:\al82dev-clang\bin"
set "OPT=-O0"
if "%1"=="O2" set "OPT=-O2"
if not exist "%DEST%" mkdir "%DEST%"
copy /y "%~dp0CppClassApi.h" "%DEST%\CppClassApi.h" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0CppClassApi.cpp" "%DEST%\CppClassApi.cpp" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0CppClassMain.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0CppClass.nsproj" "%DEST%\CppProject.nsproj" >nul
if errorlevel 1 exit /b 1
copy /y "C:\native-sharp\bin\nslink.exe" "%DEST%\nslink.exe" >nul
if errorlevel 1 exit /b 1
set "NSC_LINKER=%DEST%\nslink.exe"
set "NSC_LLVM_LIB=%TOOLS%\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
"%TOOLS%\clang.exe" --target=x86_64-pc-windows-msvc -std=c++20 -c "%DEST%\CppClassApi.cpp" -o "%DEST%\class-api.obj"
if errorlevel 1 exit /b 1
"%TOOLS%\llvm-lib.exe" /nologo /out:"%DEST%\class-api.lib" "%DEST%\class-api.obj"
if errorlevel 1 exit /b 1
"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-exe "%DEST%\CppProject.nsproj" %OPT% --no-artifact-cache --cpp-clang-root=A:\al82dev-clang -o "%DEST%\CppProject.exe"
if errorlevel 1 exit /b 1
"%DEST%\CppProject.exe"
if not errorlevel 39 exit /b 1
if errorlevel 40 exit /b 1
copy /y "%~dp0CppClassNew.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-exe "%DEST%\CppProject.nsproj" %OPT% --no-artifact-cache --cpp-clang-root=A:\al82dev-clang -o "%DEST%\unsupported.exe" > "%DEST%\new-check.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /i /c:"lifetime" "%DEST%\new-check.log" >nul
if errorlevel 1 exit /b 1
echo Proyecto NativeSharp con clase C++: resultado 39.
exit /b 0
