@echo off
setlocal
set "DEST=A:\nsc-interop-build\tanda08-matriz-volatile"
set "PATH=A:\al82dev-clang\bin;A:\nsc-interop-build\tanda1-ON\bin;%PATH%"
if not exist "%DEST%" mkdir "%DEST%"
if errorlevel 1 exit /b 1
set "TMP=%DEST%"
set "TEMP=%DEST%"
set "NSC_LINKER=A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
for %%F in (MatrixApi.h MatrixApi.c Matrix.nsproj Matrix.ns) do (
  copy /y "%~dp0%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
clang.exe -S -emit-llvm --target=x86_64-pc-windows-msvc -std=c11 -O0 "%DEST%\MatrixApi.c" -o "%DEST%\matrix-clang.ll"
if errorlevel 1 exit /b 1
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 "%DEST%\MatrixApi.c" -o "%DEST%\matrix-api.obj"
if errorlevel 1 exit /b 1
llvm-lib.exe /nologo /out:"%DEST%\matrix-api.lib" "%DEST%\matrix-api.obj"
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --dump-project-oop-llvm "%DEST%\Matrix.nsproj" > "%DEST%\matrix-nsc.ll"
if errorlevel 1 exit /b 1
python "%~dp0verificar-ir-matriz-volatile.py" "%DEST%\matrix-clang.ll" "%DEST%\matrix-nsc.ll"
if errorlevel 1 exit /b 1
for %%O in (0 1 2 3) do (
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Matrix.nsproj" -O%%O --no-artifact-cache -o "%DEST%\matrix-O%%O.exe"
  if errorlevel 1 exit /b 1
  "%DEST%\matrix-O%%O.exe"
  if errorlevel 1 exit /b 1
  echo Matriz volatile O%%O: resultado esperado cero.
)
exit /b 0
