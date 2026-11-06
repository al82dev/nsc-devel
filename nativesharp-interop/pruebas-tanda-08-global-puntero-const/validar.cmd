@echo off
setlocal
set "DEST=A:\nsc-interop-build\tanda08-global-puntero-const"
set "PATH=A:\al82dev-clang\bin;A:\nsc-interop-build\tanda1-ON\bin;%PATH%"
if not exist "%DEST%" mkdir "%DEST%"
if errorlevel 1 exit /b 1
set "TMP=%DEST%"
set "TEMP=%DEST%"
set "NSC_LINKER=A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
for %%F in (Api.h Api.c Project.nsproj Main.ns Invalid.nsproj Invalid.ns InvalidEscape.nsproj InvalidEscape.ns InvalidAddress.nsproj InvalidAddress.ns InvalidInferred.nsproj InvalidInferred.ns InvalidArgument.nsproj InvalidArgument.ns InvalidMatrix.nsproj InvalidMatrix.ns InvalidMatrixEscape.nsproj InvalidMatrixEscape.ns InvalidMatrixAddress.nsproj InvalidMatrixAddress.ns InvalidMatrixRowAddress.nsproj InvalidMatrixRowAddress.ns InvalidMatrixRowArgument.nsproj InvalidMatrixRowArgument.ns) do (
  copy /y "%~dp0%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 "%DEST%\Api.c" -o "%DEST%\api.obj"
if errorlevel 1 exit /b 1
llvm-lib.exe /nologo /out:"%DEST%\api.lib" "%DEST%\api.obj"
if errorlevel 1 exit /b 1
for %%O in (0 1 2 3) do (
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O%%O --no-artifact-cache -o "%DEST%\program-O%%O.exe"
  if errorlevel 1 exit /b 1
  "%DEST%\program-O%%O.exe"
  if errorlevel 1 exit /b 1
  echo Arreglo global de punteros a const O%%O: resultado esperado cero.
)
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Invalid.nsproj" -O0 --no-artifact-cache -o "%DEST%\invalid.exe" > "%DEST%\invalid.log" 2>&1
if not errorlevel 1 (echo Escritura indebida aceptada. & exit /b 1)
findstr /i /c:"error: left side of assignment is not assignable" "%DEST%\invalid.log" >nul
if errorlevel 1 (echo El rechazo no identifica const o readonly. & exit /b 1)
echo Escritura al dato const rechazada.
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\InvalidEscape.nsproj" -O0 --no-artifact-cache -o "%DEST%\InvalidEscape.exe" > "%DEST%\InvalidEscape.log" 2>&1
if not errorlevel 1 (echo Se acepto perdida de const por asignacion. & exit /b 1)
findstr /i /c:"would discard pointee constness" "%DEST%\InvalidEscape.log" >nul
if errorlevel 1 (echo El rechazo de asignacion no identifica el contrato. & exit /b 1)
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\InvalidAddress.nsproj" -O0 --no-artifact-cache -o "%DEST%\InvalidAddress.exe" > "%DEST%\InvalidAddress.log" 2>&1
if not errorlevel 1 (echo Se acepto perdida de const por direccion. & exit /b 1)
findstr /i /c:"cannot take the address of an array element with a const pointee" "%DEST%\InvalidAddress.log" >nul
if errorlevel 1 (echo El rechazo de direccion no identifica el contrato. & exit /b 1)
echo Perdida de const por asignacion o direccion rechazada.
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\InvalidInferred.nsproj" -O0 --no-artifact-cache -o "%DEST%\InvalidInferred.exe" > "%DEST%\InvalidInferred.log" 2>&1
if not errorlevel 1 (echo Se acepto escritura por puntero inferido. & exit /b 1)
findstr /i /c:"error: left side of assignment is not assignable" "%DEST%\InvalidInferred.log" >nul
if errorlevel 1 (echo El rechazo del puntero inferido no corresponde a la escritura. & exit /b 1)
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\InvalidArgument.nsproj" -O0 --no-artifact-cache -o "%DEST%\InvalidArgument.exe" > "%DEST%\InvalidArgument.log" 2>&1
if not errorlevel 1 (echo Se acepto el arreglo como argumento mutable. & exit /b 1)
findstr /i /c:"no viable overload of namespace function 'c_mutate' accepts the argument types" "%DEST%\InvalidArgument.log" >nul
if errorlevel 1 (echo El rechazo del argumento no corresponde a la llamada. & exit /b 1)
echo Los escapes por inferencia y argumentos se rechazaron.
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\InvalidMatrix.nsproj" -O0 --no-artifact-cache -o "%DEST%\InvalidMatrix.exe" > "%DEST%\InvalidMatrix.log" 2>&1
if not errorlevel 1 (echo Se acepto escritura mediante la matriz. & exit /b 1)
findstr /i /c:"error: left side of assignment is not assignable" "%DEST%\InvalidMatrix.log" >nul
if errorlevel 1 (echo El rechazo de escritura de matriz no corresponde a la operacion. & exit /b 1)
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\InvalidMatrixEscape.nsproj" -O0 --no-artifact-cache -o "%DEST%\InvalidMatrixEscape.exe" > "%DEST%\InvalidMatrixEscape.log" 2>&1
if not errorlevel 1 (echo Se acepto perdida de const desde matriz. & exit /b 1)
findstr /i /c:"would discard pointee constness" "%DEST%\InvalidMatrixEscape.log" >nul
if errorlevel 1 (echo El rechazo de matriz no identifica la perdida de const. & exit /b 1)
echo Escritura y escape de matriz rechazados.
for %%I in (InvalidMatrixAddress InvalidMatrixRowAddress) do (
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\%%I.nsproj" -O0 --no-artifact-cache -o "%DEST%\%%I.exe" > "%DEST%\%%I.log" 2>&1
  if not errorlevel 1 (echo Se acepto perdida de const en %%I. & exit /b 1)
  findstr /i /c:"cannot take the address of an array element with a const pointee" "%DEST%\%%I.log" >nul
  if errorlevel 1 (echo Falta diagnostico de direccion calificada en %%I. & exit /b 1)
  findstr /i /c:"bit fields have no address" "%DEST%\%%I.log" >nul
  if not errorlevel 1 (echo Diagnostico erroneo de bitfield en %%I. & exit /b 1)
)
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\InvalidMatrixRowArgument.nsproj" -O0 --no-artifact-cache -o "%DEST%\InvalidMatrixRowArgument.exe" > "%DEST%\InvalidMatrixRowArgument.log" 2>&1
if not errorlevel 1 (echo Se acepto la fila como argumento mutable. & exit /b 1)
findstr /i /c:"no viable overload of namespace function 'c_mutate' accepts the argument types" "%DEST%\InvalidMatrixRowArgument.log" >nul
if errorlevel 1 (echo El rechazo de fila no corresponde a la llamada. & exit /b 1)
echo Direcciones y argumento de fila rechazados.
exit /b 0
