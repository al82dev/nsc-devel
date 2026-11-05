@echo off
setlocal
set "DEST=A:\nsc-interop-build\cpp-fixtures\command-cpp"
set "NSC_LINKER=%DEST%\nslink.exe"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
for %%F in (CppConstArithmetic CppConstIndex) do (
    call :check %%F "left side of assignment is not assignable"
    if errorlevel 1 exit /b 1
)
for %%F in (CppConstConditional CppConstCoalesce CppConstCoalesceRight CppConstCoalesceNull CppConstIndexAddress) do (
    call :check %%F "would discard pointee constness"
    if errorlevel 1 exit /b 1
)
call :run CppConstIndexRead 21
if errorlevel 1 exit /b 1
call :run CppMutableIndex 19
if errorlevel 1 exit /b 1
call :run CppConstCoalesceRead 21
if errorlevel 1 exit /b 1
call :run CppConstIndexAddressRead 21
if errorlevel 1 exit /b 1
echo Expresiones con punteros const y mutables verificadas.
exit /b 0
:check
copy /y "%~dp0%1.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-exe "%DEST%\CppProject.nsproj" --no-artifact-cache --cpp-clang-root=A:\al82dev-clang -o "%DEST%\const-expression.exe" > "%DEST%\%1.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /c:%2 "%DEST%\%1.log" >nul
if errorlevel 1 exit /b 1
exit /b 0
:run
copy /y "%~dp0%1.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-exe "%DEST%\CppProject.nsproj" --no-artifact-cache --cpp-clang-root=A:\al82dev-clang -o "%DEST%\const-expression.exe" > "%DEST%\%1.log" 2>&1
if errorlevel 1 exit /b 1
"%DEST%\const-expression.exe"
if not errorlevel %2 exit /b 1
set /a "NEXT=%2+1"
if errorlevel %NEXT% exit /b 1
exit /b 0
