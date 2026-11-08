@echo off
setlocal
set "DEST=A:\nsc-interop-build\tanda08-atomic-global-dll-%RANDOM%-%RANDOM%"
if exist "%DEST%" exit /b 1
mkdir "%DEST%"
if errorlevel 1 goto :failure
set "PATH=A:\al82dev-clang\bin;A:\nsc-interop-build\tanda1-ON\bin;%PATH%"
set "NSC_LINKER=A:\al82dev-clang\bin\lld-link.exe"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
set "TMP=%DEST%"
set "TEMP=%DEST%"
for %%F in (Api.h Api.c Control.c Main.ns Project.nsproj) do (
  copy /y "%~dp0%%F" "%DEST%\%%F" >nul
  if errorlevel 1 goto :failure
)
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 -DBUILD_ATOMIC_DLL "%DEST%\Api.c" -o "%DEST%\api.obj"
if errorlevel 1 goto :failure
lld-link.exe /nologo /dll /noentry /nodefaultlib /out:"%DEST%\api.dll" /implib:"%DEST%\api.lib" "%DEST%\api.obj"
if errorlevel 1 goto :failure
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 "%DEST%\Control.c" -o "%DEST%\control.obj"
if errorlevel 1 goto :failure
lld-link.exe /nologo /nodefaultlib /subsystem:console /entry:mainCRTStartup /out:"%DEST%\control.exe" "%DEST%\control.obj" "%DEST%\api.lib" "A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\um\x64\kernel32.lib"
if errorlevel 1 goto :failure
"%DEST%\control.exe"
if errorlevel 1 goto :failure
nsc.exe --cpp-clang-root=A:\al82dev-clang --dump-project-oop-llvm "%DEST%\Project.nsproj" > "%DEST%\native.ll" 2> "%DEST%\native-ir.log"
if errorlevel 1 goto :failure
findstr /r /c:"load atomic i32.*@atomic_global.*seq_cst" "%DEST%\native.ll" >nul
if errorlevel 1 goto :failure
findstr /r /c:"atomicrmw add.*@atomic_global.*seq_cst" "%DEST%\native.ll" >nul
if errorlevel 1 goto :failure
findstr /r /c:"load atomic i32.*@enum_global.*seq_cst" "%DEST%\native.ll" >nul
if errorlevel 1 goto :failure
findstr /r /c:"cmpxchg.*@enum_global.*seq_cst" "%DEST%\native.ll" >nul
if errorlevel 1 goto :failure
for %%O in (0 1 2 3) do (
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O%%O --no-artifact-cache -o "%DEST%\consumer-O%%O.exe" > "%DEST%\consumer-O%%O.log" 2>&1
  if errorlevel 1 goto :failure
  "%DEST%\consumer-O%%O.exe"
  if errorlevel 1 goto :failure
  echo Global atomico importado de DLL C O%%O correcto.
)
exit /b 0
:failure
type "%DEST%\*.log"
exit /b 1
