@echo off
setlocal
set "MODE=%~1"
if "%MODE%"=="" set "MODE=global"
if /I not "%MODE%"=="global" if /I not "%MODE%"=="tls" exit /b 1
set "DEST=A:\nsc-interop-build\tanda08-atomic-enum-%MODE%-%RANDOM%-%RANDOM%"
mkdir "%DEST%"
if errorlevel 1 exit /b 1
set "PATH=A:\al82dev-clang\bin;A:\nsc-interop-build\tanda1-ON\bin;%PATH%"
set "NSC_LINKER=A:\al82dev-clang\bin\lld-link.exe"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
set "TMP=%DEST%"
set "TEMP=%DEST%"
python "%~dp0preparar.py" "%~dp0." "%DEST%" "%MODE%"
if errorlevel 1 goto :failure
copy /y "%~dp0Main.ns" "%DEST%\" >nul
copy /y "%~dp0Project.nsproj" "%DEST%\" >nul
copy /y "%~dp0..\pruebas-tanda-08-tls-c\static-global\Crt.h" "%DEST%\" >nul
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 "%DEST%\Api.c" -o "%DEST%\api.obj"
if errorlevel 1 goto :failure
llvm-lib.exe /nologo /out:"%DEST%\api.lib" "%DEST%\api.obj"
if errorlevel 1 goto :failure
clang.exe --target=x86_64-pc-windows-msvc -std=c11 -S -emit-llvm -O0 "%DEST%\Api.c" -o "%DEST%\clang.ll"
if errorlevel 1 goto :failure
nsc.exe --cpp-clang-root=A:\al82dev-clang --dump-project-oop-llvm "%DEST%\Project.nsproj" > "%DEST%\native.ll" 2> "%DEST%\native-ir.log"
if errorlevel 1 goto :failure
python "%~dp0verificar_ir.py" "%MODE%" "%DEST%\clang.ll" "%DEST%\native.ll"
if errorlevel 1 goto :failure
for %%O in (0 1 2 3) do (
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O%%O --no-artifact-cache -o "%DEST%\consumer-O%%O.exe" > "%DEST%\consumer-O%%O.log" 2>&1
  if errorlevel 1 goto :failure
  "%DEST%\consumer-O%%O.exe"
  if errorlevel 1 goto :failure
)
echo Atomica enum %MODE%: O0-O3 correctos. Evidencia: %DEST%
exit /b 0
:failure
echo Fallo. Evidencia: %DEST%
type "%DEST%\*.log"
exit /b 1
