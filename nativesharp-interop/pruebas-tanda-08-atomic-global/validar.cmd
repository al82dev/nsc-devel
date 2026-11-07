@echo off
setlocal
set "DEST=A:\nsc-interop-build\tanda08-atomic-global-validation-%RANDOM%-%RANDOM%"
if exist "%DEST%" exit /b 1
mkdir "%DEST%"
if errorlevel 1 exit /b 1
set "PATH=A:\al82dev-clang\bin;A:\nsc-interop-build\tanda1-ON\bin;%PATH%"
set "NSC_LINKER=A:\al82dev-clang\bin\lld-link.exe"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
set "TMP=%DEST%"
set "TEMP=%DEST%"
for %%F in (Api.h Api.c Probe.c Main.ns Project.nsproj) do (
  copy /y "%~dp0%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 "%DEST%\Api.c" -o "%DEST%\api.obj"
if errorlevel 1 exit /b 1
llvm-lib.exe /nologo /out:"%DEST%\api.lib" "%DEST%\api.obj"
if errorlevel 1 exit /b 1
clang.exe -S -emit-llvm --target=x86_64-pc-windows-msvc -std=c11 -O0 "%DEST%\Probe.c" -o "%DEST%\clang.ll"
if errorlevel 1 exit /b 1
findstr /r /c:"load atomic i32.*@atomic_global.*seq_cst" "%DEST%\clang.ll" >nul
if errorlevel 1 exit /b 1
findstr /r /c:"atomicrmw add.*@atomic_global.*seq_cst" "%DEST%\clang.ll" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --dump-project-oop-llvm "%DEST%\Project.nsproj" > "%DEST%\native.ll" 2> "%DEST%\native-ir.log"
if errorlevel 1 goto :failure
findstr /r /c:"load atomic i32.*@atomic_global.*seq_cst" "%DEST%\native.ll" >nul
if errorlevel 1 exit /b 1
findstr /r /c:"atomicrmw add.*@atomic_global.*seq_cst" "%DEST%\native.ll" >nul
if errorlevel 1 exit /b 1
for %%O in (0 1 2 3) do (
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O%%O --no-artifact-cache -o "%DEST%\consumer-O%%O.exe" > "%DEST%\consumer-O%%O.log" 2>&1
  if errorlevel 1 goto :failure
  "%DEST%\consumer-O%%O.exe"
  if errorlevel 1 exit /b 1
  echo Global C atomico int O%%O correcto.
)
exit /b 0
:failure
type "%DEST%\*.log"
exit /b 1
