@echo off
setlocal
set "DEST=A:\nsc-interop-build\cpp-fixtures\command-cpp-scalar"
set "TOOLS=A:\al82dev-clang\bin"
if not exist "%DEST%\temp" mkdir "%DEST%\temp"
set "TMP=%DEST%\temp"
set "TEMP=%DEST%\temp"
for %%F in (Scalar.h Scalar.cpp Project.nsproj) do copy /y "%~dp0%%F" "%DEST%\%%F" >nul
copy /y "%~dp0Main.ns.txt" "%DEST%\Main.ns" >nul
set "NSC_LINKER=C:\native-sharp\bin\nslink.exe"
set "NSC_LLVM_LIB=%TOOLS%\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
"%TOOLS%\clang.exe" --target=x86_64-pc-windows-msvc -std=c++20 -c "%DEST%\Scalar.cpp" -o "%DEST%\scalar.obj"
if errorlevel 1 goto failure
"%TOOLS%\llvm-lib.exe" /nologo /out:"%DEST%\scalar.lib" "%DEST%\scalar.obj"
if errorlevel 1 goto failure
for %%O in (0 2) do (
  "%TOOLS%\clang.exe" --target=x86_64-pc-windows-msvc -std=c++20 -O%%O -c "%~dp0ChoiceControl.cpp" -o "%DEST%\ChoiceControl-O%%O.obj"
  if errorlevel 1 goto failure
  "%TOOLS%\lld-link.exe" /nologo /nodefaultlib /entry:mainCRTStartup /subsystem:console /out:"%DEST%\ChoiceControl-O%%O.exe" "%DEST%\ChoiceControl-O%%O.obj" "%DEST%\scalar.lib" "A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\um\x64\kernel32.lib" "A:\vc2026-runtime\lib\x64\release\libcmt.lib"
  if errorlevel 1 goto failure
  "%DEST%\ChoiceControl-O%%O.exe"
  if errorlevel 68 goto failure
  if not errorlevel 67 goto failure
)
for %%S in (Main Wide Narrow Pointer ReadPointer Readonly ChoiceMutable ChoiceReadonly PairMutable PairMixed CrossFirst CrossSecond) do for %%O in (0 2) do (
  copy /y "%~dp0%%S.ns.txt" "%DEST%\Main.ns" >nul
  "A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-exe "%DEST%\Project.nsproj" -O%%O --no-artifact-cache --cpp-clang-root=A:\al82dev-clang -o "%DEST%\%%S-O%%O.exe" > "%DEST%\%%S-O%%O.log" 2>&1
  if errorlevel 1 goto failure
  "%DEST%\%%S-O%%O.exe"
  if errorlevel 68 goto failure
  if not errorlevel 67 goto failure
)
copy /y "%~dp0ReadonlyToMutable.ns.txt" "%DEST%\Main.ns" >nul
"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-exe "%DEST%\Project.nsproj" -O0 --no-artifact-cache --cpp-clang-root=A:\al82dev-clang -o "%DEST%\ReadonlyToMutable.exe" > "%DEST%\ReadonlyToMutable.log" 2>&1
if not errorlevel 1 goto failure
findstr /i /c:"no viable constructor accepts the argument types" "%DEST%\ReadonlyToMutable.log" >nul
if errorlevel 1 goto failure
copy /y "%~dp0ChoiceNull.ns.txt" "%DEST%\Main.ns" >nul
"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-exe "%DEST%\Project.nsproj" -O0 --no-artifact-cache --cpp-clang-root=A:\al82dev-clang -o "%DEST%\ChoiceNull.exe" > "%DEST%\ChoiceNull.log" 2>&1
if not errorlevel 1 goto failure
findstr /i /c:"ambiguous constructor invocation" "%DEST%\ChoiceNull.log" >nul
if errorlevel 1 goto failure
echo Constructores C++ escalares y destructores correctos en O0/O2.
exit /b 0
:failure
echo Fallo la prueba de constructor C++ con dos escalares; revisar registros en %DEST%.
exit /b 1
