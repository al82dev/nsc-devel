@echo off
setlocal
set "DEST=A:\nsc-interop-build\tanda08-atomic-enum-contencion-%RANDOM%-%RANDOM%"
mkdir "%DEST%"
if errorlevel 1 goto :failure
set "PATH=A:\al82dev-clang\bin;A:\nsc-interop-build\tanda1-ON\bin;%PATH%"
set "NSC_LINKER=A:\al82dev-clang\bin\lld-link.exe"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
set "TMP=%DEST%"
set "TEMP=%DEST%"
python "%~dp0preparar.py" "%~dp0." "%DEST%" global
if errorlevel 1 goto :failure
for %%F in (Contencion.h Contencion.c Contencion.ns Contencion.nsproj) do copy /y "%~dp0%%F" "%DEST%\" >nul
copy /y "%~dp0..\pruebas-tanda-08-tls-c\static-global\Crt.h" "%DEST%\" >nul
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 "%DEST%\Api.c" -o "%DEST%\api.obj"
if errorlevel 1 goto :failure
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 "%DEST%\Contencion.c" -o "%DEST%\contencion.obj"
if errorlevel 1 goto :failure
llvm-lib.exe /nologo /out:"%DEST%\api.lib" "%DEST%\api.obj" "%DEST%\contencion.obj"
if errorlevel 1 goto :failure
for %%O in (0 1 2 3) do (
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Contencion.nsproj" -O%%O --no-artifact-cache -o "%DEST%\contencion-O%%O.exe" > "%DEST%\contencion-O%%O.log" 2>&1
  if errorlevel 1 goto :failure
  "%DEST%\contencion-O%%O.exe"
  if errorlevel 1 goto :failure
)
echo Incrementos cruzados O0-O3 correctos. Evidencia: %DEST%
exit /b 0
:failure
echo Fallo. Evidencia: %DEST%
type "%DEST%\*.log"
exit /b 1
