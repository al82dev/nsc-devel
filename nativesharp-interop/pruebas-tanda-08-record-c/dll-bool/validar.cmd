@echo off
setlocal
set "DEST=A:\nsc-interop-build\tanda08-bool-dll"
set "PATH=A:\al82dev-clang\bin;A:\nsc-interop-build\tanda1-ON\bin;%PATH%"
set "NSC_LINKER=A:\al82dev-clang\bin\lld-link.exe"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
if not exist "%DEST%" mkdir "%DEST%"
if errorlevel 1 exit /b 1
set "TMP=%DEST%"
set "TEMP=%DEST%"
for %%F in (Api.h Api.c Control.c Main.ns Project.nsproj) do (
  copy /y "%~dp0%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 -DBUILD_BOOL_DLL "%DEST%\Api.c" -o "%DEST%\api.obj"
if errorlevel 1 exit /b 1
lld-link.exe /nologo /dll /noentry /nodefaultlib /out:"%DEST%\api.dll" /implib:"%DEST%\api.lib" "%DEST%\api.obj"
if errorlevel 1 exit /b 1
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 "%DEST%\Control.c" -o "%DEST%\control.obj"
if errorlevel 1 exit /b 1
lld-link.exe /nologo /nodefaultlib /subsystem:console /entry:mainCRTStartup /out:"%DEST%\control.exe" "%DEST%\control.obj" "%DEST%\api.lib" "A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\um\x64\kernel32.lib"
if errorlevel 1 exit /b 1
"%DEST%\control.exe"
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --dump-project-oop-llvm "%DEST%\Project.nsproj" > "%DEST%\producer.ll" 2>&1
if errorlevel 1 (type "%DEST%\producer.ll" & exit /b 1)
findstr /r /c:"load atomic volatile i8.*@flag.*acquire, align 1" "%DEST%\producer.ll" >nul
if errorlevel 1 exit /b 1
findstr /r /c:"store atomic volatile i8.*@flag.*release, align 1" "%DEST%\producer.ll" >nul
if errorlevel 1 exit /b 1
for %%O in (0 1 2 3) do (
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O%%O --no-artifact-cache -o "%DEST%\consumer-O%%O.exe" > "%DEST%\consumer-O%%O.log" 2>&1
  if errorlevel 1 (type "%DEST%\consumer-O%%O.log" & exit /b 1)
  "%DEST%\consumer-O%%O.exe"
  if errorlevel 1 exit /b 1
  echo Global C bool importado de DLL O%%O correcto.
)
exit /b 0
