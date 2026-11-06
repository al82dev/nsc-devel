@echo off
setlocal
set "DEST=A:\nsc-interop-build\tanda08-callback"
set "PATH=A:\al82dev-clang\bin;A:\nsc-interop-build\tanda1-ON\bin;%PATH%"
if not exist "%DEST%" mkdir "%DEST%"
if errorlevel 1 exit /b 1
set "TMP=%DEST%"
set "TEMP=%DEST%"
set "NSC_LINKER=A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
for %%F in (Project.nsproj Main.ns Stored.nsproj Stored.ns ConstField.nsproj ConstField.ns ConstGlobal.nsproj ConstGlobal.ns Volatile.nsproj Volatile.ns RecordCallback.nsproj RecordCallback.ns Api.h Api.c) do (
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
  echo Callback C O%%O: resultado esperado cero.
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Stored.nsproj" -O%%O --no-artifact-cache -o "%DEST%\stored-O%%O.exe"
  if errorlevel 1 exit /b 1
  "%DEST%\stored-O%%O.exe"
  if errorlevel 1 exit /b 1
  echo Callback C en campo y global O%%O: resultado esperado cero.
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\RecordCallback.nsproj" -O%%O --no-artifact-cache -o "%DEST%\record-callback-O%%O.exe"
  if errorlevel 1 exit /b 1
  "%DEST%\record-callback-O%%O.exe"
  if errorlevel 1 exit /b 1
  echo Callback C con puntero a record O%%O: resultado esperado cero.
)
for %%N in (ConstField ConstGlobal) do (
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\%%N.nsproj" -O0 --no-artifact-cache -o "%DEST%\%%N.exe" > "%DEST%\%%N.log" 2>&1
  if not errorlevel 1 exit /b 1
  findstr /c:"left side of assignment is not assignable" "%DEST%\%%N.log" >nul
  if errorlevel 1 exit /b 1
  echo Escritura de callback const %%N rechazada.
)
clang.exe -S -emit-llvm --target=x86_64-pc-windows-msvc -std=c11 -O0 "%DEST%\Api.c" -o "%DEST%\api.ll"
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --dump-project-oop-llvm "%DEST%\Volatile.nsproj" > "%DEST%\volatile.ll"
if errorlevel 1 exit /b 1
python "%~dp0verificar_ir.py" "%DEST%\api.ll" "%DEST%\volatile.ll"
if errorlevel 1 exit /b 1
for %%O in (0 1 2 3) do (
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Volatile.nsproj" -O%%O --no-artifact-cache -o "%DEST%\volatile-O%%O.exe"
  if errorlevel 1 exit /b 1
  "%DEST%\volatile-O%%O.exe"
  if errorlevel 1 exit /b 1
  echo Callback C volatile O%%O: resultado esperado cero.
)
exit /b 0
