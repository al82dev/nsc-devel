@echo off
setlocal
set "DEST=A:\nsc-interop-build\tanda08-tls-atomic-%RANDOM%-%RANDOM%"
if exist "%DEST%" exit /b 1
mkdir "%DEST%"
if errorlevel 1 exit /b 1
set "PATH=A:\nsc-interop-build\tanda1-ON\bin;A:\al82dev-clang\bin;%PATH%"
set "NSC_LINKER=A:\al82dev-clang\bin\lld-link.exe"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
set "TMP=%DEST%"
set "TEMP=%DEST%"
for %%F in (Api.h Api.c Probe.c Main.ns Project.nsproj) do (
  copy /y "%~dp0%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
copy /y "%~dp0..\static-global\Crt.h" "%DEST%\Crt.h" >nul
if errorlevel 1 exit /b 1
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 "%DEST%\Api.c" -o "%DEST%\api.obj"
if errorlevel 1 exit /b 1
llvm-lib.exe /nologo /out:"%DEST%\api.lib" "%DEST%\api.obj"
if errorlevel 1 exit /b 1
clang.exe -S -emit-llvm --target=x86_64-pc-windows-msvc -std=c11 -O0 "%DEST%\Probe.c" -o "%DEST%\probe.ll"
if errorlevel 1 exit /b 1
findstr /L /C:"load atomic" "%DEST%\probe.ll" >nul
if errorlevel 1 exit /b 1
findstr /L /C:"store atomic" "%DEST%\probe.ll" >nul
if errorlevel 1 exit /b 1
findstr /L /C:"atomicrmw" "%DEST%\probe.ll" >nul
if errorlevel 1 exit /b 1
findstr /L /C:"seq_cst" "%DEST%\probe.ll" >nul
if errorlevel 1 exit /b 1
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 "%DEST%\Probe.c" -o "%DEST%\probe.obj"
if errorlevel 1 exit /b 1
lld-link.exe /nologo /nodefaultlib /subsystem:console /entry:entry /out:"%DEST%\probe.exe" "%DEST%\probe.obj" "%DEST%\api.lib" "A:\vc2026-runtime\lib\x64\release\libcmt.lib" "A:\vc2026-runtime\lib\x64\release\libcpmt.lib" "A:\vc2026-runtime\lib\x64\release\libvcruntime.lib" "A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\ucrt\x64\libucrt.lib" "A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\um\x64\kernel32.lib"
if errorlevel 1 exit /b 1
"%DEST%\probe.exe"
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --dump-project-oop-llvm "%DEST%\Project.nsproj" > "%DEST%\native.ll" 2> "%DEST%\native-ir.log"
if errorlevel 1 (type "%DEST%\native-ir.log" & exit /b 1)
findstr /L /C:"load atomic" "%DEST%\native.ll" >nul
if errorlevel 1 exit /b 1
findstr /L /C:"store atomic" "%DEST%\native.ll" >nul
if errorlevel 1 exit /b 1
findstr /L /C:"atomicrmw" "%DEST%\native.ll" >nul
if errorlevel 1 exit /b 1
findstr /L /C:"seq_cst" "%DEST%\native.ll" >nul
if errorlevel 1 exit /b 1
for %%O in (0 1 2 3) do (
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" --no-artifact-cache -O%%O -o "%DEST%\native-O%%O.exe" > "%DEST%\native-O%%O.log" 2>&1
  if errorlevel 1 (type "%DEST%\native-O%%O.log" & exit /b 1)
  "%DEST%\native-O%%O.exe"
  if errorlevel 1 exit /b 1
)
echo TLS atomico C y NativeSharp correcto.
exit /b 0
