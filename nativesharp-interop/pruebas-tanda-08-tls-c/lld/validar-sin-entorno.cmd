@echo off
setlocal
set "DEST=A:\nsc-interop-build\tanda08-lld-sin-entorno\run-%RANDOM%-%RANDOM%"
set "PATH=A:\al82dev-clang\bin;A:\nsc-interop-build\tanda1-ON\bin;%PATH%"
set "NSC_LINKER="
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
if exist "%DEST%" exit /b 1
mkdir "%DEST%"
if errorlevel 1 exit /b 1
set "TMP=%DEST%"
set "TEMP=%DEST%"
for %%F in (Api.c Api.h Crt.h Main.ns Project.nsproj) do (
  copy /y "%~dp0..\static-global\%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
for %%F in (Producer.ns Producer.nsproj ConsumerNoHeader.ns) do (
  copy /y "%~dp0..\precompiled\%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
copy /y "%~dp0SharedProducer.nsproj" "%DEST%\SharedProducer.nsproj" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0SharedConsumer.nsproj" "%DEST%\SharedConsumer.nsproj" >nul
if errorlevel 1 exit /b 1
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 "%DEST%\Api.c" -o "%DEST%\api.obj"
if errorlevel 1 exit /b 1
llvm-lib.exe /nologo /out:"%DEST%\api.lib" "%DEST%\api.obj"
if errorlevel 1 exit /b 1
for %%O in (0 1 2 3) do (
  nsc.exe build "%DEST%\Project.nsproj" --cpp-clang-root=A:\al82dev-clang --no-restore --linker LLD -O%%O --output-format=json > "%DEST%\build-O%%O.log" 2>&1
  if errorlevel 1 (type "%DEST%\build-O%%O.log" & exit /b 1)
  findstr /L /C:"\"linker\":\"LLD\"" "%DEST%\build-O%%O.log" >nul
  if errorlevel 1 (type "%DEST%\build-O%%O.log" & exit /b 1)
  "%DEST%\bin\Debug\CTlsStaticGlobal.exe"
  if errorlevel 1 exit /b 1
  echo Build TLS O%%O con LLD sin NSC_LINKER correcto.
)
nsc.exe build "%DEST%\Producer.nsproj" --cpp-clang-root=A:\al82dev-clang --no-restore --linker LLD -O0 > "%DEST%\library.log" 2>&1
if errorlevel 1 (type "%DEST%\library.log" & exit /b 1)
if not exist "%DEST%\bin\Debug\TlsProducer.lib" exit /b 1
echo Biblioteca estatica con LLD sin NSC_LINKER correcta.
for %%O in (0 1 2 3) do (
  nsc.exe build "%DEST%\SharedProducer.nsproj" --cpp-clang-root=A:\al82dev-clang --no-restore --linker LLD -O%%O > "%DEST%\dll-O%%O.log" 2>&1
  if errorlevel 1 (type "%DEST%\dll-O%%O.log" & exit /b 1)
  if not exist "%DEST%\bin\Debug\TlsSharedProducer.dll" exit /b 1
  llvm-readobj.exe --coff-exports "%DEST%\bin\Debug\TlsSharedProducer.dll" > "%DEST%\dll-exports-O%%O.log" 2>&1
  if errorlevel 1 exit /b 1
  findstr /L /C:"__nsc_get_shared_tls" "%DEST%\dll-exports-O%%O.log" >nul
  if not errorlevel 1 exit /b 1
  findstr /L /C:"PublishedTls$2EBridge__Read" "%DEST%\dll-exports-O%%O.log" >nul
  if errorlevel 1 exit /b 1
  nsc.exe build "%DEST%\SharedConsumer.nsproj" --cpp-clang-root=A:\al82dev-clang --no-restore --linker LLD -O%%O > "%DEST%\dll-consumer-O%%O.log" 2>&1
  if errorlevel 1 (type "%DEST%\dll-consumer-O%%O.log" & exit /b 1)
  "%DEST%\bin\Debug\TlsSharedConsumer.exe"
  if errorlevel 1 exit /b 1
  echo DLL TLS O%%O con LLD sin NSC_LINKER correcta.
)
for %%I in (1 2) do (
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" --nsc-linker-selection=LLD --incremental-link --no-artifact-cache -O0 -o "%DEST%\incremental.exe" > "%DEST%\incremental-%%I.log" 2>&1
  if errorlevel 1 (type "%DEST%\incremental-%%I.log" & exit /b 1)
)
findstr /L /C:"incremental-link: hit" "%DEST%\incremental-2.log" >nul
if errorlevel 1 (type "%DEST%\incremental-2.log" & exit /b 1)
"%DEST%\incremental.exe"
if errorlevel 1 exit /b 1
echo Enlace incremental con LLD sin NSC_LINKER correcto.
exit /b 0
