@echo off
setlocal
set "DEST=A:\nsc-interop-build\tanda08-volatile-pointer-%RANDOM%-%RANDOM%"
if exist "%DEST%" exit /b 1
mkdir "%DEST%"
if errorlevel 1 goto :failure
set "PATH=A:\al82dev-clang\bin;A:\nsc-interop-build\tanda1-ON\bin;%PATH%"
set "NSC_LINKER=A:\al82dev-clang\bin\lld-link.exe"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
set "TMP=%DEST%"
set "TEMP=%DEST%"
for %%F in (Api.h Api.c Main.ns Project.nsproj ProjectISO.nsproj ProjectMicrosoft.nsproj ProjectBad.nsproj) do (
  copy /y "%~dp0%%F" "%DEST%\%%F" >nul
  if errorlevel 1 goto :failure
)
clang.exe --target=x86_64-pc-windows-msvc -std=c11 -c "%DEST%\Api.c" -o "%DEST%\api.obj"
if errorlevel 1 goto :failure
llvm-lib.exe /nologo /out:"%DEST%\api.lib" "%DEST%\api.obj"
if errorlevel 1 goto :failure
clang.exe --target=x86_64-pc-windows-msvc -std=c11 -S -emit-llvm -O0 "%DEST%\Api.c" -o "%DEST%\clang.ll"
if errorlevel 1 goto :failure
nsc.exe --cpp-clang-root=A:\al82dev-clang --dump-project-oop-llvm "%DEST%\Project.nsproj" > "%DEST%\native.ll" 2> "%DEST%\native-ir.log"
if errorlevel 1 goto :failure
python "%~dp0verificar_ir.py" "%DEST%\clang.ll" "%DEST%\native.ll"
if errorlevel 1 goto :failure
clang.exe --target=x86_64-pc-windows-msvc -std=c11 -fno-ms-volatile -S -emit-llvm -O0 "%DEST%\Api.c" -o "%DEST%\clang-iso.ll"
if errorlevel 1 goto :failure
nsc.exe --cpp-clang-root=A:\al82dev-clang --dump-project-oop-llvm "%DEST%\ProjectISO.nsproj" > "%DEST%\native-iso.ll" 2> "%DEST%\native-iso-ir.log"
if errorlevel 1 goto :failure
python "%~dp0verificar_ir_iso.py" "%DEST%\clang-iso.ll" "%DEST%\native-iso.ll"
if errorlevel 1 goto :failure
nsc.exe --cpp-clang-root=A:\al82dev-clang --dump-project-oop-llvm "%DEST%\ProjectMicrosoft.nsproj" > "%DEST%\native-ms.ll" 2> "%DEST%\native-ms-ir.log"
if errorlevel 1 goto :failure
python "%~dp0verificar_ir.py" "%DEST%\clang.ll" "%DEST%\native-ms.ll"
if errorlevel 1 goto :failure
nsc.exe --cpp-clang-root=A:\al82dev-clang --check-project-oop-hir "%DEST%\ProjectBad.nsproj" > "%DEST%\bad-mode.log" 2>&1
if not errorlevel 1 goto :failure
findstr /c:"VolatileMode must be Default, Microsoft or ISO" "%DEST%\bad-mode.log" >nul
if errorlevel 1 goto :failure
for %%O in (0 1 2 3) do (
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O%%O --no-artifact-cache -o "%DEST%\consumer-O%%O.exe" > "%DEST%\consumer-O%%O.log" 2>&1
  if errorlevel 1 goto :failure
  "%DEST%\consumer-O%%O.exe"
  if errorlevel 1 goto :failure
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\ProjectISO.nsproj" -O%%O --no-artifact-cache -o "%DEST%\consumer-iso-O%%O.exe" > "%DEST%\consumer-iso-O%%O.log" 2>&1
  if errorlevel 1 goto :failure
  "%DEST%\consumer-iso-O%%O.exe"
  if errorlevel 1 goto :failure
  echo Punteros C volatile O%%O correctos.
)
echo IR disponible en %DEST%
exit /b 0
:failure
echo Fallo; evidencia en %DEST%
type "%DEST%\*.log"
exit /b 1
