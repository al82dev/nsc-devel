@echo off
setlocal
set "DEST=A:\nsc-interop-build\tanda08-bool-dynamic-%RANDOM%-%RANDOM%"
if exist "%DEST%" exit /b 1
mkdir "%DEST%"
if errorlevel 1 goto :failure
set "PATH=A:\al82dev-clang\bin;A:\nsc-interop-build\tanda1-ON\bin;%PATH%"
set "NSC_LINKER=A:\al82dev-clang\bin\lld-link.exe"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
set "TMP=%DEST%"
set "TEMP=%DEST%"
for %%F in (A.c B.c Main.ns Project.nsproj) do (
  copy /y "%~dp0%%F" "%DEST%\%%F" >nul
  if errorlevel 1 goto :failure
)
for %%F in (A B) do (
  clang.exe --target=x86_64-pc-windows-msvc -std=c11 -c "%DEST%\%%F.c" -o "%DEST%\%%F.obj"
  if errorlevel 1 goto :failure
  lld-link.exe /dll /noentry /nodefaultlib /out:"%DEST%\%%F.dll" "%DEST%\%%F.obj"
  if errorlevel 1 goto :failure
)
clang.exe --target=x86_64-pc-windows-msvc -std=c11 -S -emit-llvm -O0 "%DEST%\A.c" -o "%DEST%\clang.ll"
if errorlevel 1 goto :failure
nsc.exe --dump-project-oop-llvm "%DEST%\Project.nsproj" > "%DEST%\native.ll" 2> "%DEST%\native-ir.log"
if errorlevel 1 goto :failure
python "%~dp0verificar_ir.py" "%DEST%\clang.ll" "%DEST%\native.ll"
if errorlevel 1 goto :failure
for %%O in (0 1 2 3) do (
  nsc.exe --emit-project-oop-exe "%DEST%\Project.nsproj" -O%%O --no-artifact-cache -o "%DEST%\consumer-O%%O.exe" > "%DEST%\consumer-O%%O.log" 2>&1
  if errorlevel 1 goto :failure
  "%DEST%\consumer-O%%O.exe"
  if errorlevel 1 goto :failure
  echo Importacion dinamica bool O%%O correcta.
)
echo IR disponible en %DEST%
exit /b 0
:failure
echo Fallo; evidencia en %DEST%
type "%DEST%\*.log"
exit /b 1
