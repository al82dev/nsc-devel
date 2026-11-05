@echo off
setlocal
call "%~dp0ejecutar.cmd"
if errorlevel 1 exit /b 1
set "DEST=A:\nsc-interop-build\cpp-fixtures\command-cpp-implicit-lifetime"
if not exist "%DEST%\temp" mkdir "%DEST%\temp"
set "TMP=%DEST%\temp"
set "TEMP=%DEST%\temp"
set "NSC_LINKER=C:\native-sharp\bin\nslink.exe"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
copy /y "%~dp0Project-lib.nsproj" "%DEST%\Project-lib.nsproj" >nul
if errorlevel 1 exit /b 1
"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-lib "%DEST%\Project-lib.nsproj" -O0 --no-artifact-cache --cpp-clang-root=A:\al82dev-clang -o "%DEST%\NativeImplicit.lib"
exit /b %errorlevel%
