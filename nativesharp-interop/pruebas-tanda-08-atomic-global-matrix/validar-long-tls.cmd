@echo off
setlocal
set "DEST=A:\nsc-interop-build\tanda08-atomic-long-tls-%RANDOM%-%RANDOM%"
if exist "%DEST%" exit /b 1
mkdir "%DEST%"
if errorlevel 1 exit /b 1
set "PATH=A:\al82dev-clang\bin;A:\nsc-interop-build\tanda1-ON\bin;%PATH%"
set "NSC_LINKER=A:\al82dev-clang\bin\lld-link.exe"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
set "TMP=%DEST%"
set "TEMP=%DEST%"
for %%F in (LongTls.h LongTls.c LongTls.ns LongTls.nsproj) do (
  copy /y "%~dp0%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
copy /y "%~dp0..\pruebas-tanda-08-tls-c\static-global\Crt.h" "%DEST%\Crt.h" >nul
if errorlevel 1 exit /b 1
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 "%DEST%\LongTls.c" -o "%DEST%\long-tls.obj"
if errorlevel 1 exit /b 1
llvm-lib.exe /nologo /out:"%DEST%\long-tls.lib" "%DEST%\long-tls.obj"
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --dump-project-oop-llvm "%DEST%\LongTls.nsproj" > "%DEST%\native.ll" 2> "%DEST%\native-ir.log"
if errorlevel 1 (type "%DEST%\native-ir.log" & exit /b 1)
for %%V in (tls_clong tls_culong) do (
  findstr /r /c:"@%%V = external thread_local.*i32" "%DEST%\native.ll" >nul
  if errorlevel 1 exit /b 1
  findstr /r /c:"load atomic i32.*@%%V.*seq_cst" "%DEST%\native.ll" >nul
  if errorlevel 1 exit /b 1
  findstr /r /c:"atomicrmw.*@%%V.*seq_cst" "%DEST%\native.ll" >nul
  if errorlevel 1 exit /b 1
)
for %%O in (0 1 2 3) do (
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\LongTls.nsproj" -O%%O --no-artifact-cache -o "%DEST%\consumer-O%%O.exe" > "%DEST%\consumer-O%%O.log" 2>&1
  if errorlevel 1 (type "%DEST%\consumer-O%%O.log" & exit /b 1)
  "%DEST%\consumer-O%%O.exe"
  if errorlevel 1 exit /b 1
  echo Escalares TLS atomicos long C O%%O correctos.
)
exit /b 0
