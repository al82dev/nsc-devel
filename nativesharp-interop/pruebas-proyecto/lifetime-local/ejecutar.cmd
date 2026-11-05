@echo off
setlocal
set "DEST=A:\nsc-interop-build\cpp-fixtures\command-cpp-local"
set "TOOLS=A:\al82dev-clang\bin"
if not exist "%DEST%\temp" mkdir "%DEST%\temp"
set "TMP=%DEST%\temp"
set "TEMP=%DEST%\temp"
for %%F in (Ready.h Ready.cpp Project.nsproj) do copy /y "%~dp0%%F" "%DEST%\%%F" >nul
set "NSC_LINKER=C:\native-sharp\bin\nslink.exe"
set "NSC_LLVM_LIB=%TOOLS%\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
"%TOOLS%\clang.exe" --target=x86_64-pc-windows-msvc -std=c++20 -c "%DEST%\Ready.cpp" -o "%DEST%\ready.obj"
if errorlevel 1 goto failure
"%TOOLS%\llvm-lib.exe" /nologo /out:"%DEST%\ready.lib" "%DEST%\ready.obj"
if errorlevel 1 goto failure
for %%S in (Scope Return Exception Break Continue) do for %%O in (0 2) do (
  copy /y "%~dp0%%S.ns.txt" "%DEST%\Main.ns" >nul
  "A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-exe "%DEST%\Project.nsproj" -O%%O --no-artifact-cache --cpp-clang-root=A:\al82dev-clang -o "%DEST%\%%S-O%%O.exe" > "%DEST%\%%S-O%%O.log" 2>&1
  if errorlevel 1 goto failure
  "%DEST%\%%S-O%%O.exe"
  if errorlevel 68 goto failure
  if not errorlevel 67 goto failure
)
copy /y "%~dp0Throwing.ns.txt" "%DEST%\Main.ns" >nul
"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-exe "%DEST%\Project.nsproj" -O0 --no-artifact-cache --cpp-clang-root=A:\al82dev-clang -o "%DEST%\Throwing.exe" > "%DEST%\Throwing.log" 2>&1
if not errorlevel 1 goto failure
findstr /c:"non-throwing constructor and destructor" "%DEST%\Throwing.log" >nul
if errorlevel 1 goto failure
copy /y "%~dp0Copy.ns.txt" "%DEST%\Main.ns" >nul
"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-exe "%DEST%\Project.nsproj" -O0 --no-artifact-cache --cpp-clang-root=A:\al82dev-clang -o "%DEST%\Copy.exe" > "%DEST%\Copy.log" 2>&1
if not errorlevel 1 goto failure
findstr /c:"NS3020" "%DEST%\Copy.log" >nul
if errorlevel 1 goto failure
copy /y "%~dp0Move.ns.txt" "%DEST%\Main.ns" >nul
"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-exe "%DEST%\Project.nsproj" -O0 --no-artifact-cache --cpp-clang-root=A:\al82dev-clang -o "%DEST%\Move.exe" > "%DEST%\Move.log" 2>&1
if not errorlevel 1 goto failure
findstr /i /c:"C++ local" "%DEST%\Move.log" >nul
if errorlevel 1 goto failure
echo Locales C++ noexcept: ambito, retorno, excepcion, break y continue O0/O2 correctos; copia y move rechazados.
exit /b 0
:failure
echo Fallo la prueba de locales C++; revisar los registros en %DEST%.
exit /b 1
