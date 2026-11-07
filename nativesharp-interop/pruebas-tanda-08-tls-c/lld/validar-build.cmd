@echo off
setlocal
set "DEST=A:\nsc-interop-build\tanda08-tls-build-lld"
set "PATH=A:\al82dev-clang\bin;A:\nsc-interop-build\tanda1-ON\bin;%PATH%"
set "NSC_LINKER=A:\al82dev-clang\bin\lld-link.exe"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
if not exist "%DEST%" mkdir "%DEST%"
if errorlevel 1 exit /b 1
set "TMP=%DEST%"
set "TEMP=%DEST%"
for %%F in (Api.h Api.c Main.ns) do (
  copy /y "%~dp0..\%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
for %%F in (Crt.h ProjectLld.nsproj) do (
  copy /y "%~dp0%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 "%DEST%\Api.c" -o "%DEST%\api.obj"
if errorlevel 1 exit /b 1
llvm-lib.exe /nologo /out:"%DEST%\api.lib" "%DEST%\api.obj"
if errorlevel 1 exit /b 1
for %%O in (0 1 2 3) do (
  nsc.exe build "%DEST%\ProjectLld.nsproj" --cpp-clang-root=A:\al82dev-clang --no-restore --linker LLD -O%%O --output-format=json > "%DEST%\build-O%%O.json" 2>&1
  if errorlevel 1 exit /b 1
  findstr /L /C:"\"linker\":\"LLD\"" "%DEST%\build-O%%O.json" >nul
  if errorlevel 1 exit /b 1
  "%DEST%\bin\Debug\CTlsPrerequisite.exe"
  if errorlevel 1 exit /b 1
  echo TLS C con nsc build y LLD O%%O: correcto.
)
exit /b 0
