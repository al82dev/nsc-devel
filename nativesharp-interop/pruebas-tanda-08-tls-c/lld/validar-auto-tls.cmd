@echo off
setlocal
set "DEST=A:\nsc-interop-build\tanda08-lld-auto\run-%RANDOM%-%RANDOM%"
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
for %%F in (Plain.ns Plain.nsproj NoTls.c NoTls.h NoTls.ns NoTls.nsproj) do (
  copy /y "%~dp0%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
mkdir "%DEST%\precompiled"
if errorlevel 1 exit /b 1
for %%F in (Producer.ns Producer.nsproj ConsumerNoHeader.ns ConsumerNoHeader.nsproj) do (
  copy /y "%~dp0..\precompiled\%%F" "%DEST%\precompiled\%%F" >nul
  if errorlevel 1 exit /b 1
)
for %%F in (Api.c Api.h Crt.h) do (
  copy /y "%~dp0..\static-global\%%F" "%DEST%\precompiled\%%F" >nul
  if errorlevel 1 exit /b 1
)
type nul > "%DEST%\Crt.h"
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 "%DEST%\Api.c" -o "%DEST%\api.obj"
if errorlevel 1 exit /b 1
llvm-lib.exe /nologo /out:"%DEST%\api.lib" "%DEST%\api.obj"
if errorlevel 1 exit /b 1
for %%O in (0 1 2 3) do (
  nsc.exe build "%DEST%\Project.nsproj" --cpp-clang-root=A:\al82dev-clang --no-restore -O%%O --output-format=json > "%DEST%\auto-O%%O.log" 2>&1
  if errorlevel 1 (type "%DEST%\auto-O%%O.log" & exit /b 1)
  findstr /L /C:"\"linker\":\"LLD\"" "%DEST%\auto-O%%O.log" >nul
  if errorlevel 1 (type "%DEST%\auto-O%%O.log" & exit /b 1)
  "%DEST%\bin\Debug\CTlsPrerequisite.exe"
  if errorlevel 1 exit /b 1
  echo TLS interno seleccion automatica O%%O correcto.
)
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 "%DEST%\precompiled\Api.c" -o "%DEST%\precompiled\api.obj"
if errorlevel 1 exit /b 1
llvm-lib.exe /nologo /out:"%DEST%\precompiled\api.lib" "%DEST%\precompiled\api.obj"
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-lib "%DEST%\precompiled\Producer.nsproj" --nsc-linker-selection=LLD --no-artifact-cache -O0 -o "%DEST%\precompiled\TlsProducer.lib" > "%DEST%\producer.log" 2>&1
if errorlevel 1 (type "%DEST%\producer.log" & exit /b 1)
nsc.exe build "%DEST%\precompiled\ConsumerNoHeader.nsproj" --cpp-clang-root=A:\al82dev-clang --no-restore -O0 --output-format=json > "%DEST%\precompiled.log" 2>&1
if errorlevel 1 (type "%DEST%\precompiled.log" & exit /b 1)
findstr /L /C:"\"linker\":\"LLD\"" "%DEST%\precompiled.log" >nul
if errorlevel 1 (type "%DEST%\precompiled.log" & exit /b 1)
"%DEST%\precompiled\bin\Debug\TlsConsumerNoHeader.exe"
if errorlevel 1 exit /b 1
echo TLS precompilado seleccion automatica correcto.
nsc.exe build "%DEST%\Plain.nsproj" --no-restore -O0 --output-format=json > "%DEST%\plain.log" 2>&1
if errorlevel 1 (type "%DEST%\plain.log" & exit /b 1)
findstr /L /C:"\"linker\":\"NSLink\"" "%DEST%\plain.log" >nul
if errorlevel 1 (type "%DEST%\plain.log" & exit /b 1)
"%DEST%\bin\Debug\LinkerCachePlain.exe"
if errorlevel 1 exit /b 1
echo Proyecto sin TLS conserva NSLink.
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 "%DEST%\NoTls.c" -o "%DEST%\notls.obj"
if errorlevel 1 exit /b 1
llvm-lib.exe /nologo /out:"%DEST%\notls.lib" "%DEST%\notls.obj"
if errorlevel 1 exit /b 1
nsc.exe build "%DEST%\NoTls.nsproj" --cpp-clang-root=A:\al82dev-clang --no-restore -O0 --output-format=json > "%DEST%\notls.log" 2>&1
if errorlevel 1 (type "%DEST%\notls.log" & exit /b 1)
findstr /L /C:"\"linker\":\"NSLink\"" "%DEST%\notls.log" >nul
if errorlevel 1 (type "%DEST%\notls.log" & exit /b 1)
"%DEST%\bin\Debug\NoTlsStatic.exe"
if errorlevel 1 exit /b 1
echo Biblioteca C sin TLS conserva NSLink.
nsc.exe build "%DEST%\Project.nsproj" --cpp-clang-root=A:\al82dev-clang --no-restore --linker NSLink -O0 > "%DEST%\explicit.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /L /C:"NSLink failed" "%DEST%\explicit.log" >nul
if errorlevel 1 (type "%DEST%\explicit.log" & exit /b 1)
echo Seleccion explicita de NSLink respetada.
set "NSC_LINKER=A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
nsc.exe build "%DEST%\Project.nsproj" --cpp-clang-root=A:\al82dev-clang --no-restore -O0 > "%DEST%\env.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /L /C:"NSLink failed" "%DEST%\env.log" >nul
if errorlevel 1 (type "%DEST%\env.log" & exit /b 1)
set "NSC_LINKER="
echo NSC_LINKER explicito respetado.
> "%DEST%\Project.nspm.json" echo {"schemaVersion":1,"build":{"linker":"NSLink"},"packages":[]}
nsc.exe build "%DEST%\Project.nsproj" --cpp-clang-root=A:\al82dev-clang --no-restore -O0 > "%DEST%\sidecar.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /L /C:"NSLink failed" "%DEST%\sidecar.log" >nul
if errorlevel 1 (type "%DEST%\sidecar.log" & exit /b 1)
echo Seleccion de NSLink en sidecar respetada.
exit /b 0
