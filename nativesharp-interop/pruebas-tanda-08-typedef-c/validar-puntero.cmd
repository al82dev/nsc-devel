@echo off
setlocal
set "DEST=A:\nsc-interop-build\tanda08-typedef-c"
set "PATH=A:\al82dev-clang\bin;A:\nsc-interop-build\tanda1-ON\bin;%PATH%"
set "TMP=%DEST%"
set "TEMP=%DEST%"
set "NSC_LINKER=A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
if not exist "%DEST%\api.lib" exit /b 1
for %%O in (0 1 2 3) do (
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\MatrixPointer.nsproj" -O%%O --no-artifact-cache -o "%DEST%\pointer-O%%O.exe"
  if errorlevel 1 exit /b 1
  "%DEST%\pointer-O%%O.exe"
  if errorlevel 1 exit /b 1
  echo Puntero a matriz typedef O%%O ejecutado correctamente.
)
for %%I in (InvalidConstMatrix InvalidMatrixSize) do (
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\%%I.nsproj" -O0 --no-artifact-cache -o "%DEST%\%%I.exe" > "%DEST%\%%I.log" 2>&1
  if not errorlevel 1 (echo Se acepto argumento invalido en %%I. & exit /b 1)
  findstr /i /c:"no viable overload of namespace function 'c_matrix_mutate' accepts the argument types" "%DEST%\%%I.log" >nul
  if errorlevel 1 (echo El rechazo de %%I no corresponde a la llamada. & exit /b 1)
)
echo Argumentos de matriz incompatibles rechazados.
echo MATRIZ PUNTERO TYPEDEF COMPLETA
exit /b 0
