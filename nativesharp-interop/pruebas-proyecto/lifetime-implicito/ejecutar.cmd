@echo off
setlocal
set "DEST=A:\nsc-interop-build\cpp-fixtures\command-cpp-implicit-lifetime"
set "TOOLS=A:\al82dev-clang\bin"
if not exist "%DEST%" mkdir "%DEST%"
if not exist "%DEST%\temp" mkdir "%DEST%\temp"
set "TMP=%DEST%\temp"
set "TEMP=%DEST%\temp"
copy /y "%~dp0Api.h" "%DEST%\Api.h" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0Api.cpp" "%DEST%\Api.cpp" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0Project.nsproj" "%DEST%\Project.nsproj" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0Main.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
set "NSC_LINKER=C:\native-sharp\bin\nslink.exe"
set "NSC_LLVM_LIB=%TOOLS%\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
"%TOOLS%\clang.exe" --target=x86_64-pc-windows-msvc -std=c++20 -c "%DEST%\Api.cpp" -o "%DEST%\api.obj"
if errorlevel 1 exit /b 1
"%TOOLS%\llvm-lib.exe" /nologo /out:"%DEST%\api.lib" "%DEST%\api.obj"
if errorlevel 1 exit /b 1
"%TOOLS%\llvm-nm.exe" --defined-only "%DEST%\api.lib" > "%DEST%\library-symbols.txt"
if errorlevel 1 exit /b 1
findstr /l /c:"??0Outer@Demo@@" /c:"??1Outer@Demo@@" "%DEST%\library-symbols.txt" >nul
if not errorlevel 1 exit /b 1
for %%O in (0 2) do (
  "A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-exe "%DEST%\Project.nsproj" -O%%O --no-artifact-cache --cpp-clang-root=A:\al82dev-clang -o "%DEST%\Implicit-O%%O.exe"
  if errorlevel 1 exit /b 1
  "%DEST%\Implicit-O%%O.exe"
  if not errorlevel 73 exit /b 1
  if errorlevel 74 exit /b 1
)
echo Proyecto NativeSharp con lifetime implicito C++: resultado 73.
exit /b 0
