@echo off
setlocal
set "DEST=A:\nsc-interop-build\cpp-fixtures\command-cpp-class-pointers"
set "TOOLS=A:\al82dev-clang\bin"
set "OPT=-O0"
if "%1"=="O2" set "OPT=-O2"
if not exist "%DEST%" mkdir "%DEST%"
copy /y "%~dp0CppClassPointersApi.h" "%DEST%\CppClassApi.h" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0CppClassPointersApi.cpp" "%DEST%\CppClassApi.cpp" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0CppClassPointersMain.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0CppClass.nsproj" "%DEST%\CppProject.nsproj" >nul
if errorlevel 1 exit /b 1
copy /y "C:\native-sharp\bin\nslink.exe" "%DEST%\nslink.exe" >nul
if errorlevel 1 exit /b 1
set "NSC_LINKER=%DEST%\nslink.exe"
set "NSC_LLVM_LIB=%TOOLS%\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
"%TOOLS%\clang.exe" --target=x86_64-pc-windows-msvc -std=c++20 -c "%DEST%\CppClassApi.cpp" -o "%DEST%\class-api.obj"
if errorlevel 1 exit /b 1
"%TOOLS%\llvm-lib.exe" /nologo /out:"%DEST%\class-api.lib" "%DEST%\class-api.obj"
if errorlevel 1 exit /b 1
"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-exe "%DEST%\CppProject.nsproj" %OPT% --no-artifact-cache --cpp-clang-root=A:\al82dev-clang -o "%DEST%\CppProject.exe"
if errorlevel 1 exit /b 1
"%DEST%\CppProject.exe"
if not errorlevel 141 exit /b 1
if errorlevel 142 exit /b 1
copy /y "%~dp0CppClassPointersConst.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-exe "%DEST%\CppProject.nsproj" %OPT% --no-artifact-cache --cpp-clang-root=A:\al82dev-clang -o "%DEST%\const-unsupported.exe" > "%DEST%\const-check.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /i /c:"readonly" /c:"read-only" /c:"assignable" "%DEST%\const-check.log" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0CppClassPointersReadonlyArgument.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-exe "%DEST%\CppProject.nsproj" %OPT% --no-artifact-cache --cpp-clang-root=A:\al82dev-clang -o "%DEST%\readonly-argument.exe" > "%DEST%\readonly-argument.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /i /c:"readonly" /c:"read-only" /c:"conversion" /c:"overload" /c:"viable" "%DEST%\readonly-argument.log" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0CppClassPointersReadonlyRecord.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-exe "%DEST%\CppProject.nsproj" %OPT% --no-artifact-cache --cpp-clang-root=A:\al82dev-clang -o "%DEST%\readonly-record.exe" > "%DEST%\readonly-record.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /i /c:"readonly" /c:"read-only" /c:"assignable" "%DEST%\readonly-record.log" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0CppClassPointersNullOverload.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-exe "%DEST%\CppProject.nsproj" %OPT% --no-artifact-cache --cpp-clang-root=A:\al82dev-clang -o "%DEST%\null-overload.exe" > "%DEST%\null-overload.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /i /c:"ambiguous" "%DEST%\null-overload.log" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0CppClassPointersReadonlyReceiver.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-exe "%DEST%\CppProject.nsproj" %OPT% --no-artifact-cache --cpp-clang-root=A:\al82dev-clang -o "%DEST%\readonly-receiver.exe" > "%DEST%\readonly-receiver.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /i /c:"readonly" /c:"read-only" /c:"viable" "%DEST%\readonly-receiver.log" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0CppClassPointersCrossAmbiguous.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-exe "%DEST%\CppProject.nsproj" %OPT% --no-artifact-cache --cpp-clang-root=A:\al82dev-clang -o "%DEST%\cross-ambiguous.exe" > "%DEST%\cross-ambiguous.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /i /c:"ambiguous" "%DEST%\cross-ambiguous.log" >nul
if errorlevel 1 exit /b 1
for %%F in (CppFieldReadonlyPointee CppFieldReadonlyPointer CppFieldDiscardConst CppStaticFieldReadonlyPointee CppStaticFieldReadonlyPointer CppStaticFieldDiscardConst CppRecordFieldReadonly CppStaticRecordFieldReadonly CppRecordFieldDiscardConst CppStaticRecordFieldDiscardConst) do (
  copy /y "%~dp0%%F.ns.txt" "%DEST%\Main.ns" >nul
  if errorlevel 1 exit /b 1
  "A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-exe "%DEST%\CppProject.nsproj" %OPT% --no-artifact-cache --cpp-clang-root=A:\al82dev-clang -o "%DEST%\%%F.exe" > "%DEST%\%%F.log" 2>&1
  if not errorlevel 1 exit /b 1
  findstr /i /c:"readonly" /c:"read-only" /c:"assignable" /c:"const" /c:"mutability" "%DEST%\%%F.log" >nul
  if errorlevel 1 exit /b 1
)
echo Metodos C++ con punteros y sobrecargas: resultado 141.
exit /b 0
