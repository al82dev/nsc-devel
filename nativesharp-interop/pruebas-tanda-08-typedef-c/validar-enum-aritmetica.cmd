@echo off
setlocal
set "DEST=A:\nsc-interop-build\tanda08-typedef-c"
set "PATH=A:\al82dev-clang\bin;A:\nsc-interop-build\tanda1-ON\bin;%PATH%"
set "TMP=%DEST%"
set "TEMP=%DEST%"
set "NSC_LINKER=A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
for %%O in (0 1 2 3) do (
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\EnumArithmetic.nsproj" -O%%O --no-artifact-cache -o "%DEST%\enum-arithmetic-O%%O.exe"
  if errorlevel 1 exit /b 1
  "%DEST%\enum-arithmetic-O%%O.exe"
  if errorlevel 1 exit /b 1
  echo Aritmetica enum C O%%O ejecutada correctamente.
)
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\InvalidNativeEnumArithmetic.nsproj" -O0 --no-artifact-cache -o "%DEST%\InvalidNativeEnumArithmetic.exe" > "%DEST%\InvalidNativeEnumArithmetic.log" 2>&1
if not errorlevel 1 (echo Se acepto aritmetica en enum NativeSharp. & exit /b 1)
findstr /i /c:"binary operator requires compatible numeric operands" "%DEST%\InvalidNativeEnumArithmetic.log" >nul
if errorlevel 1 (echo El rechazo no corresponde a la aritmetica del enum NS. & exit /b 1)
echo ARITMETICA ENUM C COMPLETA
exit /b 0
