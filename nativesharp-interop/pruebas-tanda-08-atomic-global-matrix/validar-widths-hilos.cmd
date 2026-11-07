@echo off
setlocal
set "DEST=A:\nsc-interop-build\tanda08-atomic-widths-hilos-%RANDOM%-%RANDOM%"
if exist "%DEST%" exit /b 1
mkdir "%DEST%"
if errorlevel 1 exit /b 1
set "PATH=A:\al82dev-clang\bin;A:\nsc-interop-build\tanda1-ON\bin;%PATH%"
set "NSC_LINKER=A:\al82dev-clang\bin\lld-link.exe"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
set "TMP=%DEST%"
set "TEMP=%DEST%"
python "%~dp0preparar-widths.py" "%~dp0." "%DEST%" tls
if errorlevel 1 exit /b 1
for %%F in (WidthsThread.h WidthsThread.c WidthsThread.ns WidthsThread.nsproj) do (
  copy /y "%~dp0%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
copy /y "%~dp0..\pruebas-tanda-08-tls-c\static-global\Crt.h" "%DEST%\Crt.h" >nul
if errorlevel 1 exit /b 1
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 "%DEST%\Widths.c" -o "%DEST%\widths.obj"
if errorlevel 1 exit /b 1
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 "%DEST%\WidthsThread.c" -o "%DEST%\widths-thread.obj"
if errorlevel 1 exit /b 1
llvm-lib.exe /nologo /out:"%DEST%\widths.lib" "%DEST%\widths.obj" "%DEST%\widths-thread.obj"
if errorlevel 1 exit /b 1
for %%O in (0 1 2 3) do (
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\WidthsThread.nsproj" -O%%O --no-artifact-cache -o "%DEST%\thread-O%%O.exe" > "%DEST%\thread-O%%O.log" 2>&1
  if errorlevel 1 goto :failure
  "%DEST%\thread-O%%O.exe"
  if errorlevel 1 exit /b 1
  echo Aislamiento TLS punteros atomicos anchos O%%O correcto.
)
exit /b 0
:failure
type "%DEST%\*.log"
exit /b 1
