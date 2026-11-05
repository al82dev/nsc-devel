@echo off
setlocal
set "DEST=A:\nsc-interop-build\cpp-fixtures\command-cpp-implicit-lifetime-dll"
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
"%TOOLS%\clang.exe" --target=x86_64-pc-windows-msvc -std=c++20 -DBUILDING_API -c "%DEST%\Api.cpp" -o "%DEST%\api.obj"
if errorlevel 1 exit /b 1
"%TOOLS%\lld-link.exe" /nologo /dll /noentry /nodefaultlib /out:"%DEST%\api.dll" /implib:"%DEST%\api.lib" "%DEST%\api.obj"
if errorlevel 1 exit /b 1
for %%O in (0 2) do (
  "A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-exe "%DEST%\Project.nsproj" -O%%O --no-artifact-cache --cpp-clang-root=A:\al82dev-clang -o "%DEST%\Implicit-O%%O.exe"
  if errorlevel 1 exit /b 1
  "%TOOLS%\llvm-readobj.exe" --coff-imports "%DEST%\Implicit-O%%O.exe" > "%DEST%\imports-O%%O.txt"
  if errorlevel 1 exit /b 1
  findstr /l /c:"Name: api.dll" "%DEST%\imports-O%%O.txt" >nul
  if errorlevel 1 exit /b 1
  findstr /l /c:"??0Member@Demo@@QEAA@XZ" "%DEST%\imports-O%%O.txt" >nul
  if errorlevel 1 exit /b 1
  findstr /l /c:"??1Member@Demo@@QEAA@XZ" "%DEST%\imports-O%%O.txt" >nul
  if errorlevel 1 exit /b 1
  "%DEST%\Implicit-O%%O.exe"
  if not errorlevel 73 exit /b 1
  if errorlevel 74 exit /b 1
)
echo Proyecto NativeSharp con lifetime implicito C++ en DLL: resultado 73.
exit /b 0
