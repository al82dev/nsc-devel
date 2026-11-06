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
for %%F in (OpaqueApi.h OpaqueApi.c OpaqueConsumer.ns OpaqueConsumer.nsproj OpaqueInvalid.ns OpaqueInvalid.nsproj) do (
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
)
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\OpaqueInvalid.nsproj" -O0 --no-artifact-cache -o "%DEST%\invalid.exe" > "%DEST%\invalid.log" 2>&1
if not errorlevel 1 (echo Objeto incompleto por valor aceptado. & exit /b 1)
findstr /i /c:"error:" "%DEST%\invalid.log" >nul
if errorlevel 1 (echo Falta diagnostico de objeto incompleto. & exit /b 1)
echo Objeto incompleto por valor rechazado; revisar diagnostico.
exit /b 0
