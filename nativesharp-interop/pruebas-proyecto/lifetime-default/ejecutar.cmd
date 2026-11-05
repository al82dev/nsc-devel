@echo off
setlocal
set "DEST=A:\nsc-interop-build\cpp-fixtures\command-cpp-default"
set "TOOLS=A:\al82dev-clang\bin"
if not exist "%DEST%\temp" mkdir "%DEST%\temp"
set "TMP=%DEST%\temp"
set "TEMP=%DEST%\temp"
for %%F in (Default.h Default.cpp Project.nsproj) do copy /y "%~dp0%%F" "%DEST%\%%F" >nul
copy /y "%~dp0Main.ns.txt" "%DEST%\Main.ns" >nul
set "NSC_LINKER=C:\native-sharp\bin\nslink.exe"
set "NSC_LLVM_LIB=%TOOLS%\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
"%TOOLS%\clang.exe" --target=x86_64-pc-windows-msvc -std=c++20 -c "%DEST%\Default.cpp" -o "%DEST%\default.obj"
if errorlevel 1 goto failure
"%TOOLS%\llvm-lib.exe" /nologo /out:"%DEST%\default.lib" "%DEST%\default.obj"
if errorlevel 1 goto failure
for %%O in (0 2) do (
  "A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-exe "%DEST%\Project.nsproj" -O%%O --no-artifact-cache --cpp-clang-root=A:\al82dev-clang -o "%DEST%\Default-O%%O.exe" > "%DEST%\Default-O%%O.log" 2>&1
  if errorlevel 1 goto failure
  "%DEST%\Default-O%%O.exe"
  if errorlevel 68 goto failure
  if not errorlevel 67 goto failure
)
echo Constructor C++ explicito sin argumentos y destructor correctos en O0/O2.
exit /b 0
:failure
echo Fallo la prueba de constructor C++ sin argumentos; revisar registros en %DEST%.
exit /b 1
