@echo off
setlocal
set "DEST=A:\nsc-interop-build\tanda08-static-complejas-%RANDOM%-%RANDOM%"
if exist "%DEST%" exit /b 1
mkdir "%DEST%"
if errorlevel 1 exit /b 1
set "PATH=A:\al82dev-clang\bin;A:\nsc-interop-build\tanda1-ON\bin;%PATH%"
set "NSC_LINKER=A:\al82dev-clang\bin\lld-link.exe"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
set "TMP=%DEST%"
set "TEMP=%DEST%"
for %%F in (Api.h Control.c Main.ns Project.nsproj Negative.ns Negative.nsproj) do (
  copy /y "%~dp0%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
clang.exe --target=x86_64-pc-windows-msvc -std=c11 -c "%DEST%\Control.c" -o "%DEST%\control.obj"
if errorlevel 1 exit /b 1
lld-link.exe /entry:main /subsystem:console /nodefaultlib /out:"%DEST%\control.exe" "%DEST%\control.obj"
if errorlevel 1 exit /b 1
"%DEST%\control.exe"
if errorlevel 1 exit /b 1
echo Control C de globales internas complejas correcto.
clang.exe --target=x86_64-pc-windows-msvc -std=c11 -S -emit-llvm -O0 "%DEST%\Control.c" -o "%DEST%\control.ll"
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --dump-project-oop-llvm "%DEST%\Project.nsproj" > "%DEST%\native.ll" 2> "%DEST%\native-ir.log"
if errorlevel 1 exit /b 1
python "%~dp0..\pruebas-tanda-08-global-static-agregados\verificar-ir.py" "%DEST%\control.ll" "%DEST%\native.ll"
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --check-project-method-bodies "%DEST%\Negative.nsproj" > "%DEST%\negative.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /c:"error: left side of assignment is not assignable" "%DEST%\negative.log" >nul
if errorlevel 1 exit /b 1
findstr /c:"readonly_values[0] = 30" "%DEST%\negative.log" >nul
if errorlevel 1 exit /b 1
echo Escritura en arreglo const rechazada.
for %%O in (0 1 2 3) do (
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O%%O --no-artifact-cache -o "%DEST%\consumer-O%%O.exe" > "%DEST%\consumer-O%%O.log" 2>&1
  if errorlevel 1 goto :failure
  "%DEST%\consumer-O%%O.exe"
  if errorlevel 1 exit /b 1
  echo Globales internas complejas O%%O correctas.
)
exit /b 0
:failure
type "%DEST%\consumer-O*.log"
exit /b 1
