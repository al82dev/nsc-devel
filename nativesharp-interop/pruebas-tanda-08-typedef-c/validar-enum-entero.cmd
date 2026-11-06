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
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\EnumToInteger.nsproj" -O%%O --no-artifact-cache -o "%DEST%\enum-to-int-O%%O.exe"
  if errorlevel 1 exit /b 1
  "%DEST%\enum-to-int-O%%O.exe"
  if errorlevel 1 exit /b 1
  echo Enum C a entero O%%O ejecutado correctamente.
)
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\InvalidNativeEnumToInteger.nsproj" -O0 --no-artifact-cache -o "%DEST%\InvalidNativeEnumToInteger.exe" > "%DEST%\InvalidNativeEnumToInteger.log" 2>&1
if not errorlevel 1 (echo Se acepto conversion del enum NativeSharp a entero. & exit /b 1)
findstr /i /c:"cannot initialize local 'number'" "%DEST%\InvalidNativeEnumToInteger.log" >nul
if errorlevel 1 (echo El rechazo no corresponde a la conversion del enum NativeSharp. & exit /b 1)
echo ENUM C A ENTERO COMPLETO
exit /b 0
