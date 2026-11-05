@echo off
setlocal
set "DEST=A:\nsc-interop-build\cpp-fixtures\command-cpp-lifetime"
set "TOOLS=A:\al82dev-clang\bin"
if not exist "%DEST%" mkdir "%DEST%"
if not exist "%DEST%\temp" mkdir "%DEST%\temp"
set "TMP=%DEST%\temp"
set "TEMP=%DEST%\temp"
copy /y "%~dp0Ready.h" "%DEST%\Ready.h" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0Ready.cpp" "%DEST%\Ready.cpp" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0Project.nsproj" "%DEST%\Project.nsproj" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0Main.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
set "NSC_LINKER=C:\native-sharp\bin\nslink.exe"
set "NSC_LLVM_LIB=%TOOLS%\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
"%TOOLS%\clang.exe" --target=x86_64-pc-windows-msvc -std=c++20 -c "%DEST%\Ready.cpp" -o "%DEST%\ready.obj"
if errorlevel 1 exit /b 1
"%TOOLS%\llvm-lib.exe" /nologo /out:"%DEST%\ready.lib" "%DEST%\ready.obj"
if errorlevel 1 exit /b 1
for %%O in (0 2) do (
  "A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-exe "%DEST%\Project.nsproj" -O%%O --no-artifact-cache --cpp-clang-root=A:\al82dev-clang -o "%DEST%\Ready-O%%O.exe"
  if errorlevel 1 exit /b 1
  "%DEST%\Ready-O%%O.exe"
  if not errorlevel 67 exit /b 1
  if errorlevel 68 exit /b 1
)
copy /y "%~dp0Unsupported.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-exe "%DEST%\Project.nsproj" -O0 --no-artifact-cache --cpp-clang-root=A:\al82dev-clang -o "%DEST%\Unsupported.exe" > "%DEST%\unsupported.log" 2>&1
if not errorlevel 1 exit /b 1
echo Proyecto NativeSharp con constructor y destructor C++: resultado 67.
exit /b 0
