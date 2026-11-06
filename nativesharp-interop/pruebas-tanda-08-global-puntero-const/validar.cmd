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
for %%F in (Api.h Api.c Project.nsproj Main.ns Invalid.nsproj Invalid.ns) do (
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
findstr /i /c:"const" /c:"readonly" "%DEST%\invalid.log" >nul
if errorlevel 1 (echo El rechazo no identifica const o readonly. & exit /b 1)
echo Escritura al dato const rechazada.
exit /b 0
