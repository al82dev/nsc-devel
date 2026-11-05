@echo off
setlocal
set "DEST=A:\nsc-interop-build\tanda08-incompleto"
set "PATH=A:\al82dev-clang\bin;A:\nsc-interop-build\tanda1-ON\bin;%PATH%"
if not exist "%DEST%" mkdir "%DEST%"
if errorlevel 1 exit /b 1
set "TMP=%DEST%"
set "TEMP=%DEST%"
set "NSC_LINKER=A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
copy /y "%~dp0Project.nsproj" "%DEST%\Project.nsproj" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0Api.h" "%DEST%\Api.h" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0Pointer.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O0 --no-artifact-cache -o "%DEST%\pointer.exe"
if errorlevel 1 exit /b 1
"%DEST%\pointer.exe"
if errorlevel 1 exit /b 1
copy /y "%~dp0Sizeof.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O0 --no-artifact-cache -o "%DEST%\invalid.exe" > "%DEST%\incomplete.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /i /c:"incomplete" "%DEST%\incomplete.log" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0SizeofArray.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O0 --no-artifact-cache -o "%DEST%\invalid-array.exe" > "%DEST%\incomplete-array.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /i /c:"unknown bound" "%DEST%\incomplete-array.log" >nul
if errorlevel 1 exit /b 1
echo Tipo incompleto C: puntero valido y sizeof rechazado.
exit /b 0
