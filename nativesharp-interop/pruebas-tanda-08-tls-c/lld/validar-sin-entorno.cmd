@echo off
setlocal
set "DEST=A:\nsc-interop-build\tanda08-lld-sin-entorno"
set "PATH=A:\al82dev-clang\bin;A:\nsc-interop-build\tanda1-ON\bin;%PATH%"
set "NSC_LINKER="
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
if not exist "%DEST%" mkdir "%DEST%"
if errorlevel 1 exit /b 1
set "TMP=%DEST%"
set "TEMP=%DEST%"
for %%F in (Api.c Api.h Crt.h Main.ns Project.nsproj) do (
  copy /y "%~dp0..\static-global\%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
for %%F in (Producer.ns Producer.nsproj) do (
  copy /y "%~dp0..\precompiled\%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 "%DEST%\Api.c" -o "%DEST%\api.obj"
if errorlevel 1 exit /b 1
llvm-lib.exe /nologo /out:"%DEST%\api.lib" "%DEST%\api.obj"
if errorlevel 1 exit /b 1
nsc.exe build "%DEST%\Project.nsproj" --cpp-clang-root=A:\al82dev-clang --no-restore --linker LLD -O0 --output-format=json > "%DEST%\build.log" 2>&1
if errorlevel 1 (type "%DEST%\build.log" & exit /b 1)
findstr /L /C:"\"linker\":\"LLD\"" "%DEST%\build.log" >nul
if errorlevel 1 (type "%DEST%\build.log" & exit /b 1)
"%DEST%\bin\Debug\CTlsStaticGlobal.exe"
if errorlevel 1 exit /b 1
echo Build TLS con LLD sin NSC_LINKER correcto.
nsc.exe build "%DEST%\Producer.nsproj" --cpp-clang-root=A:\al82dev-clang --no-restore --linker LLD -O0 > "%DEST%\library.log" 2>&1
if errorlevel 1 (type "%DEST%\library.log" & exit /b 1)
if not exist "%DEST%\bin\Debug\TlsProducer.lib" exit /b 1
echo Biblioteca estatica con LLD sin NSC_LINKER correcta.
exit /b 0
