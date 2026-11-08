@echo off
setlocal
set "MODE=%~1"
if "%MODE%"=="" set "MODE=global"
if /I not "%MODE%"=="global" if /I not "%MODE%"=="tls" exit /b 1
set "DEST=A:\nsc-interop-build\tanda08-atomic-widths-%MODE%-%RANDOM%-%RANDOM%"
if exist "%DEST%" exit /b 1
mkdir "%DEST%"
if errorlevel 1 goto :failure
set "PATH=A:\al82dev-clang\bin;A:\nsc-interop-build\tanda1-ON\bin;%PATH%"
set "NSC_LINKER=A:\al82dev-clang\bin\lld-link.exe"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
set "TMP=%DEST%"
set "TEMP=%DEST%"
python "%~dp0preparar-widths.py" "%~dp0." "%DEST%" "%MODE%"
if errorlevel 1 goto :failure
for %%F in (Widths.ns Widths.nsproj) do (
  copy /y "%~dp0%%F" "%DEST%\%%F" >nul
  if errorlevel 1 goto :failure
)
copy /y "%~dp0..\pruebas-tanda-08-tls-c\static-global\Crt.h" "%DEST%\Crt.h" >nul
if errorlevel 1 goto :failure
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 "%DEST%\Widths.c" -o "%DEST%\widths.obj"
if errorlevel 1 goto :failure
llvm-lib.exe /nologo /out:"%DEST%\widths.lib" "%DEST%\widths.obj"
if errorlevel 1 goto :failure
nsc.exe --cpp-clang-root=A:\al82dev-clang --dump-project-oop-llvm "%DEST%\Widths.nsproj" > "%DEST%\native.ll" 2> "%DEST%\native-ir.log"
if errorlevel 1 goto :failure
python "%~dp0verificar-widths.py" "%DEST%\native.ll" "%MODE%"
if errorlevel 1 goto :failure
for %%O in (0 1 2 3) do (
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Widths.nsproj" -O%%O --no-artifact-cache -o "%DEST%\consumer-O%%O.exe" > "%DEST%\consumer-O%%O.log" 2>&1
  if errorlevel 1 goto :failure
  "%DEST%\consumer-O%%O.exe"
  if errorlevel 1 goto :failure
  echo Punteros atomicos %MODE% anchos adicionales O%%O correctos.
)
exit /b 0
:failure
type "%DEST%\*.log"
exit /b 1
