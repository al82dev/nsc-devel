@echo off
setlocal
set "DEST=A:\nsc-interop-build\cpp-fixtures\command"
set "TOOLS=A:\al82dev-clang\bin"
if not exist "%DEST%" mkdir "%DEST%"
copy /y "%~dp0api.c" "%DEST%\api.c" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0api.h" "%DEST%\api.h" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0Main.ns" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0Comando.nsproj" "%DEST%\CppProject.nsproj" >nul
if errorlevel 1 exit /b 1
copy /y "C:\native-sharp\bin\nslink.exe" "%DEST%\nslink.exe" >nul
if errorlevel 1 exit /b 1
set "NSC_LINKER=%DEST%\nslink.exe"
set "NSC_LLVM_LIB=%TOOLS%\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
"%TOOLS%\clang.exe" --target=x86_64-pc-windows-msvc -c "%DEST%\api.c" -o "%DEST%\api.obj"
if errorlevel 1 exit /b 1
"%TOOLS%\llvm-lib.exe" /nologo /out:"%DEST%\api.lib" "%DEST%\api.obj"
if errorlevel 1 exit /b 1
"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-exe "%DEST%\CppProject.nsproj" --no-artifact-cache --cpp-clang-root=A:\al82dev-clang -o "%DEST%\CppProject.exe"
if errorlevel 1 exit /b 1
"%DEST%\CppProject.exe"
if not errorlevel 85 exit /b 1
if errorlevel 86 exit /b 1
"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-exe "%DEST%\CppProject.nsproj" --artifact-cache="%DEST%\cache" --cpp-clang-root=A:\al82dev-clang -o "%DEST%\cache-test.exe" > "%DEST%\cache-check.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /c:"require --no-artifact-cache" "%DEST%\cache-check.log" >nul
if errorlevel 1 exit /b 1
echo Proyecto C y NativeSharp ejecutado: resultado 85.
exit /b 0
