@echo off
setlocal
set "DEST=A:\nsc-interop-build\tanda06"
if not exist "%DEST%" mkdir "%DEST%"
set "NSC_LINKER=A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-exe "%~dp0Plain.nsproj" -O0 --no-artifact-cache -o "%DEST%\plain-on.exe"
if errorlevel 1 exit /b 1
"%DEST%\plain-on.exe"
if errorlevel 1 exit /b 1
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-OFF\generated\NativeSharp.Runtime.Win32.lib"
"A:\nsc-interop-build\tanda1-OFF\bin\nsc.exe" --emit-project-oop-exe "%~dp0Plain.nsproj" -O0 --no-artifact-cache -o "%DEST%\plain-off.exe"
if errorlevel 1 exit /b 1
"%DEST%\plain-off.exe"
if errorlevel 1 exit /b 1
"A:\al82dev-clang\bin\llvm-readobj.exe" --file-headers --coff-imports "%DEST%\plain-on.exe" > "%DEST%\imports-on.txt"
if errorlevel 1 exit /b 1
"A:\al82dev-clang\bin\llvm-readobj.exe" --file-headers --coff-imports "%DEST%\plain-off.exe" > "%DEST%\imports-off.txt"
if errorlevel 1 exit /b 1
echo Ejecutables ON y OFF compilados y ejecutados. Importaciones en %DEST%.
exit /b 0
