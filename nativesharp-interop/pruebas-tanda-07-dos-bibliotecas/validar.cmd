@echo off
setlocal
set "DEST=A:\nsc-interop-build\tanda07-two-libraries"
if not exist "%DEST%" mkdir "%DEST%"
set "TMP=%DEST%"
set "TEMP=%DEST%"
set "NSC_LINKER=A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
copy /y "%~dp0Project.nsproj" "%DEST%\Project.nsproj" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0Main.ns" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0a.h" "%DEST%\a.h" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0b.h" "%DEST%\b.h" >nul
if errorlevel 1 exit /b 1
"A:\al82dev-clang\bin\clang.exe" -c --target=x86_64-pc-windows-msvc "%~dp0a.c" -o "%DEST%\a.obj"
if errorlevel 1 exit /b 1
"A:\al82dev-clang\bin\clang.exe" -c --target=x86_64-pc-windows-msvc "%~dp0b.c" -o "%DEST%\b.obj"
if errorlevel 1 exit /b 1
"A:\al82dev-clang\bin\llvm-lib.exe" /out:"%DEST%\a.lib" "%DEST%\a.obj"
if errorlevel 1 exit /b 1
"A:\al82dev-clang\bin\llvm-lib.exe" /out:"%DEST%\b.lib" "%DEST%\b.obj"
if errorlevel 1 exit /b 1
"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O0 --no-artifact-cache -o "%DEST%\program.exe"
if errorlevel 1 exit /b 1
"%DEST%\program.exe"
if errorlevel 1 exit /b 1
echo Dos bibliotecas C compiladas, enlazadas y ejecutadas con configuraciones independientes.
exit /b 0
