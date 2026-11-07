@echo off
setlocal
set "DEST=A:\nsc-interop-build\tanda08-tls-bitcode\run-%RANDOM%-%RANDOM%"
if exist "%DEST%" exit /b 1
mkdir "%DEST%"
if errorlevel 1 exit /b 1
set "PATH=A:\nsc-interop-build\tanda1-ON\bin;A:\al82dev-clang\bin;A:\nsc-interop-build\cpp-fixtures\command-cpp;%PATH%"
set "NSC_LINKER="
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
set "TMP=%DEST%"
set "TEMP=%DEST%"
for %%F in (Api.c Api.h Main.ns Project.nsproj) do (
  copy /y "%~dp0..\%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
copy /y "%~dp0ProjectNoCrt.nsproj" "%DEST%\ProjectNoCrt.nsproj" >nul
if errorlevel 1 exit /b 1
for %%F in (NoTls.c NoTls.h NoTls.ns NoTls.nsproj) do (
  copy /y "%~dp0%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
type nul > "%DEST%\Crt.h"
for %%L in (thin full) do (
  clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 -flto=%%L "%DEST%\Api.c" -o "%DEST%\api.obj"
  if errorlevel 1 exit /b 1
  llvm-dis.exe "%DEST%\api.obj" -o "%DEST%\api.ll"
  if errorlevel 1 exit /b 1
  findstr /C:"thread_local" "%DEST%\api.ll" >nul
  if errorlevel 1 exit /b 1
  llvm-lib.exe /nologo /out:"%DEST%\api.lib" "%DEST%\api.obj"
  if errorlevel 1 exit /b 1
  llvm-nm.exe --undefined-only "%DEST%\api.lib" > "%DEST%\symbols-%%L.log"
  if errorlevel 1 exit /b 1
  findstr /L /C:"_tls_index" "%DEST%\symbols-%%L.log" >nul
  if not errorlevel 1 exit /b 1
  nsc.exe build "%DEST%\ProjectNoCrt.nsproj" --cpp-clang-root=A:\al82dev-clang --no-restore -O0 > "%DEST%\no-crt-%%L.log" 2>&1
  if not errorlevel 1 exit /b 1
  findstr /L /C:"lld-link: error" "%DEST%\no-crt-%%L.log" >nul
  if errorlevel 1 (type "%DEST%\no-crt-%%L.log" & exit /b 1)
  findstr /L /C:"undefined symbol: _tls_index" "%DEST%\no-crt-%%L.log" >nul
  if errorlevel 1 (type "%DEST%\no-crt-%%L.log" & exit /b 1)
  nsc.exe build "%DEST%\Project.nsproj" --cpp-clang-root=A:\al82dev-clang --no-restore -O0 --output-format=json > "%DEST%\crt-%%L.log" 2>&1
  if errorlevel 1 (type "%DEST%\crt-%%L.log" & exit /b 1)
  findstr /L /C:"\"linker\":\"LLD\"" "%DEST%\crt-%%L.log" >nul
  if errorlevel 1 (type "%DEST%\crt-%%L.log" & exit /b 1)
  "%DEST%\bin\Debug\CTlsPrerequisite.exe"
  if errorlevel 1 exit /b 1
  echo TLS %%L detectado y ejecutado correctamente.
  clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 -flto=%%L "%DEST%\NoTls.c" -o "%DEST%\notls.obj"
  if errorlevel 1 exit /b 1
  llvm-dis.exe "%DEST%\notls.obj" -o "%DEST%\notls.ll"
  if errorlevel 1 exit /b 1
  findstr /C:"thread_local" "%DEST%\notls.ll" >nul
  if not errorlevel 1 exit /b 1
  llvm-lib.exe /nologo /out:"%DEST%\notls.lib" "%DEST%\notls.obj"
  if errorlevel 1 exit /b 1
  nsc.exe build "%DEST%\NoTls.nsproj" --cpp-clang-root=A:\al82dev-clang --no-restore -O0 --output-format=json > "%DEST%\no-tls-%%L.log" 2>&1
  if errorlevel 1 (type "%DEST%\no-tls-%%L.log" & exit /b 1)
  findstr /L /C:"\"linker\":\"LLD\"" "%DEST%\no-tls-%%L.log" >nul
  if errorlevel 1 (type "%DEST%\no-tls-%%L.log" & exit /b 1)
  "%DEST%\bin\Debug\NoTlsStatic.exe"
  if errorlevel 1 exit /b 1
  echo Bitcode %%L sin TLS enlazado y ejecutado correctamente.
)
exit /b 0
