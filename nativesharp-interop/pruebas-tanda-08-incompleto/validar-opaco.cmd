@echo off
setlocal
set "DEST=A:\nsc-interop-build\tanda08-opaco-c"
set "PATH=A:\al82dev-clang\bin;A:\nsc-interop-build\tanda1-ON\bin;%PATH%"
if not exist "%DEST%" mkdir "%DEST%"
if errorlevel 1 exit /b 1
set "TMP=%DEST%"
set "TEMP=%DEST%"
set "NSC_LINKER=A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
for %%F in (OpaqueApi.h OpaqueApi.c OpaqueConsumer.ns OpaqueConsumer.nsproj OpaqueInvalid.ns OpaqueInvalid.nsproj OpaqueUnionInvalid.ns OpaqueUnionInvalid.nsproj OpaqueProducer.ns OpaqueProducer.nsproj OpaquePublished.ns OpaquePublished.nsproj) do (
  copy /y "%~dp0%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 "%DEST%\OpaqueApi.c" -o "%DEST%\opaque.obj"
if errorlevel 1 exit /b 1
llvm-lib.exe /nologo /out:"%DEST%\opaque.lib" "%DEST%\opaque.obj"
if errorlevel 1 exit /b 1
for %%O in (0 1 2 3) do (
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\OpaqueConsumer.nsproj" -O%%O --no-artifact-cache -o "%DEST%\opaque-O%%O.exe"
  if errorlevel 1 exit /b 1
  "%DEST%\opaque-O%%O.exe"
  if errorlevel 1 exit /b 1
  echo Punteros a tipo incompleto O%%O: resultado esperado cero.
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-lib "%DEST%\OpaqueProducer.nsproj" -O%%O --no-artifact-cache -o "%DEST%\COpaqueProducer.lib"
  if errorlevel 1 exit /b 1
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\OpaquePublished.nsproj" -O%%O --no-artifact-cache -o "%DEST%\published-O%%O.exe"
  if errorlevel 1 exit /b 1
  "%DEST%\published-O%%O.exe"
  if errorlevel 1 exit /b 1
  echo Punteros opacos precompilados O%%O: resultado esperado cero.
)
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\OpaqueInvalid.nsproj" -O0 --no-artifact-cache -o "%DEST%\invalid.exe" > "%DEST%\invalid.log" 2>&1
if not errorlevel 1 (echo Objeto incompleto por valor aceptado. & exit /b 1)
findstr /i /c:"C object local 'value' cannot be created by value because its type is incomplete" "%DEST%\invalid.log" >nul
if errorlevel 1 (echo Falta diagnostico de objeto incompleto. & exit /b 1)
echo Objeto por valor rechazado por tipo incompleto.
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\OpaqueUnionInvalid.nsproj" -O0 --no-artifact-cache -o "%DEST%\invalid-union.exe" > "%DEST%\invalid-union.log" 2>&1
if not errorlevel 1 (echo Union incompleta por valor aceptada. & exit /b 1)
findstr /i /c:"C object local 'value' cannot be created by value because its type is incomplete" "%DEST%\invalid-union.log" >nul
if errorlevel 1 (echo Falta diagnostico especifico de union incompleta. & exit /b 1)
echo Union por valor rechazada por tipo incompleto.
exit /b 0
