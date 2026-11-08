@echo off
setlocal
set "DEST=A:\nsc-interop-build\tanda08-atomic-global-thread-%RANDOM%-%RANDOM%"
if exist "%DEST%" exit /b 1
mkdir "%DEST%"
if errorlevel 1 goto :failure
set "PATH=A:\al82dev-clang\bin;A:\nsc-interop-build\tanda1-ON\bin;%PATH%"
set "NSC_LINKER=A:\al82dev-clang\bin\lld-link.exe"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
set "TMP=%DEST%"
set "TEMP=%DEST%"
for %%F in (Api.h Api.c Thread.h Thread.c Thread.ns Thread.nsproj) do (
  copy /y "%~dp0%%F" "%DEST%\%%F" >nul
  if errorlevel 1 goto :failure
)
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 "%DEST%\Api.c" -o "%DEST%\api.obj"
if errorlevel 1 goto :failure
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 "%DEST%\Thread.c" -o "%DEST%\thread.obj"
if errorlevel 1 goto :failure
clang.exe -S -emit-llvm -O0 --target=x86_64-pc-windows-msvc -std=c11 "%DEST%\Thread.c" -o "%DEST%\thread-clang.ll"
if errorlevel 1 goto :failure
llvm-lib.exe /nologo /out:"%DEST%\api.lib" "%DEST%\api.obj" "%DEST%\thread.obj"
if errorlevel 1 goto :failure
nsc.exe --cpp-clang-root=A:\al82dev-clang --dump-project-oop-llvm "%DEST%\Thread.nsproj" > "%DEST%\thread-native.ll" 2> "%DEST%\thread-ir.log"
if errorlevel 1 goto :failure
python "%~dp0verificar-hilos-ir.py" "%DEST%\thread-clang.ll" "%DEST%\thread-native.ll"
if errorlevel 1 goto :failure
for %%O in (0 1 2 3) do (
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Thread.nsproj" -O%%O --no-artifact-cache -o "%DEST%\consumer-O%%O.exe" > "%DEST%\consumer-O%%O.log" 2>&1
  if errorlevel 1 goto :failure
  "%DEST%\consumer-O%%O.exe"
  if errorlevel 1 goto :failure
  echo Global atomico compartido entre hilos O%%O correcto.
)
echo Evidencia en %DEST%
exit /b 0
:failure
type "%DEST%\*.log"
exit /b 1
