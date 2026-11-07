@echo off
setlocal
set "DEST=A:\nsc-interop-build\tanda08-sidecar-nuevo\run-%RANDOM%-%RANDOM%"
if exist "%DEST%" exit /b 1
set "PATH=A:\nsc-interop-build\tanda1-ON\bin;A:\al82dev-clang\bin;A:\nsc-interop-build\cpp-fixtures\command-cpp;%PATH%"
set "NSC_LINKER="
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
nsc.exe new console -n Project -o "%DEST%"
if errorlevel 1 exit /b 1
set "TMP=%DEST%"
set "TEMP=%DEST%"
findstr /L /C:"\"linker\"" "%DEST%\Project.nspm.json" >nul
if not errorlevel 1 exit /b 1
for %%F in (Api.c Api.h Main.ns Project.nsproj) do (
  copy /y "%~dp0..\%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
type nul > "%DEST%\Crt.h"
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 "%DEST%\Api.c" -o "%DEST%\api.obj"
if errorlevel 1 exit /b 1
llvm-lib.exe /nologo /out:"%DEST%\api.lib" "%DEST%\api.obj"
if errorlevel 1 exit /b 1
nsc.exe add package Dummy --version 1.0.0 "%DEST%\Project.nsproj"
if errorlevel 1 exit /b 1
findstr /L /C:"\"linker\"" "%DEST%\Project.nspm.json" >nul
if not errorlevel 1 exit /b 1
nsc.exe remove package Dummy "%DEST%\Project.nsproj"
if errorlevel 1 exit /b 1
findstr /L /C:"\"linker\"" "%DEST%\Project.nspm.json" >nul
if not errorlevel 1 exit /b 1
nsc.exe build "%DEST%\Project.nsproj" --cpp-clang-root=A:\al82dev-clang --no-restore -O0 --output-format=json > "%DEST%\build.log" 2>&1
if errorlevel 1 (type "%DEST%\build.log" & exit /b 1)
findstr /L /C:"\"linker\":\"LLD\"" "%DEST%\build.log" >nul
if errorlevel 1 (type "%DEST%\build.log" & exit /b 1)
"%DEST%\bin\Debug\CTlsPrerequisite.exe"
if not "%ERRORLEVEL%"=="0" exit /b 1
for %%L in (NSLink LLD) do (
  > "%DEST%\Project.nspm.json" echo {"schemaVersion":1,"build":{"linker":"%%L"},"packages":[]}
  nsc.exe add package Dummy --version 1.0.0 "%DEST%\Project.nsproj"
  if errorlevel 1 exit /b 1
  findstr /L /C:"\"linker\": \"%%L\"" "%DEST%\Project.nspm.json" >nul
  if errorlevel 1 exit /b 1
  nsc.exe remove package Dummy "%DEST%\Project.nsproj"
  if errorlevel 1 exit /b 1
  findstr /L /C:"\"linker\": \"%%L\"" "%DEST%\Project.nspm.json" >nul
  if errorlevel 1 exit /b 1
)
echo Sidecar nuevo y reescrito conserva seleccion automatica y explicita.
exit /b 0
