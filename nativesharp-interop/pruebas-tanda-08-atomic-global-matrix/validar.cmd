@echo off
setlocal
set "DEST=A:\nsc-interop-build\tanda08-atomic-global-matrix-validation-%RANDOM%-%RANDOM%"
if exist "%DEST%" exit /b 1
mkdir "%DEST%"
if errorlevel 1 exit /b 1
set "PATH=A:\al82dev-clang\bin;A:\nsc-interop-build\tanda1-ON\bin;%PATH%"
set "NSC_LINKER=A:\al82dev-clang\bin\lld-link.exe"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
set "TMP=%DEST%"
set "TEMP=%DEST%"
for %%F in (Api.h Api.c Probe.c Main.ns Project.nsproj Compound.ns Compound.nsproj CompoundProbe.c) do (
  copy /y "%~dp0%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 "%DEST%\Api.c" -o "%DEST%\api.obj"
if errorlevel 1 exit /b 1
llvm-lib.exe /nologo /out:"%DEST%\api.lib" "%DEST%\api.obj"
if errorlevel 1 exit /b 1
clang.exe -S -emit-llvm --target=x86_64-pc-windows-msvc -std=c11 -O0 "%DEST%\Probe.c" -o "%DEST%\clang.ll"
if errorlevel 1 exit /b 1
python "%~dp0verificar_ir.py" "%DEST%\clang.ll"
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --dump-project-oop-llvm "%DEST%\Project.nsproj" > "%DEST%\native.ll" 2> "%DEST%\native-ir.log"
if errorlevel 1 (type "%DEST%\native-ir.log" & exit /b 1)
python "%~dp0verificar_ir.py" "%DEST%\native.ll"
if errorlevel 1 exit /b 1
clang.exe -S -emit-llvm --target=x86_64-pc-windows-msvc -std=c11 -O0 "%DEST%\CompoundProbe.c" -o "%DEST%\compound-clang.ll"
if errorlevel 1 exit /b 1
python "%~dp0verificar_compuesto.py" "%DEST%\compound-clang.ll"
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --dump-project-oop-llvm "%DEST%\Compound.nsproj" > "%DEST%\compound-native.ll" 2> "%DEST%\compound-ir.log"
if errorlevel 1 (type "%DEST%\compound-ir.log" & exit /b 1)
python "%~dp0verificar_compuesto.py" "%DEST%\compound-native.ll"
if errorlevel 1 exit /b 1
for %%O in (0 1 2 3) do (
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O%%O --no-artifact-cache -o "%DEST%\consumer-O%%O.exe" > "%DEST%\consumer-O%%O.log" 2>&1
  if errorlevel 1 (type "%DEST%\consumer-O%%O.log" & exit /b 1)
  "%DEST%\consumer-O%%O.exe"
  if errorlevel 1 exit /b 1
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Compound.nsproj" -O%%O --no-artifact-cache -o "%DEST%\compound-O%%O.exe" > "%DEST%\compound-O%%O.log" 2>&1
  if errorlevel 1 (type "%DEST%\compound-O%%O.log" & exit /b 1)
  "%DEST%\compound-O%%O.exe"
  if errorlevel 1 exit /b 1
  echo Matriz de globales atomicos O%%O correcta.
)
exit /b 0
