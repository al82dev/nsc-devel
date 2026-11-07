@echo off
setlocal
set "DEST=A:\nsc-interop-build\tls-abi\transitive-test-%RANDOM%-%RANDOM%"
if exist "%DEST%" exit /b 1
mkdir "%DEST%"
if errorlevel 1 exit /b 1
set "PATH=A:\nsc-interop-build\tanda1-ON\bin;A:\al82dev-clang\bin;A:\nsc-interop-build\cpp-fixtures\command-cpp;%PATH%"
set "NSC_LINKER="
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
set "TMP=%DEST%"
set "TEMP=%DEST%"
for %%F in (TransitiveProducer.ns TransitiveProducer.nsproj TransitiveConsumer.ns TransitiveConsumer.nsproj) do copy /y "%~dp0%%F" "%DEST%\%%F" >nul
for %%F in (Api.c Api.h Crt.h) do copy /y "%~dp0..\static-global\%%F" "%DEST%\%%F" >nul
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 "%DEST%\Api.c" -o "%DEST%\api.obj"
if errorlevel 1 exit /b 1
llvm-lib.exe /nologo /out:"%DEST%\api.lib" "%DEST%\api.obj"
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-lib "%DEST%\TransitiveProducer.nsproj" -O0 --no-artifact-cache -o "%DEST%\TlsProducer.lib"
if errorlevel 1 exit /b 1
llvm-nm.exe --undefined-only "%DEST%\TlsProducer.lib" > "%DEST%\symbols.txt"
if errorlevel 1 exit /b 1
findstr /L /C:"_tls_index" "%DEST%\symbols.txt" >nul
if not errorlevel 1 exit /b 1
for %%O in (0 1 2 3) do (
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\TransitiveConsumer.nsproj" -O%%O --no-artifact-cache -o "%DEST%\consumer-O%%O.exe"
  if errorlevel 1 exit /b 1
  "%DEST%\consumer-O%%O.exe"
  if errorlevel 1 exit /b 1
)
nsc.exe build "%DEST%\TransitiveConsumer.nsproj" --cpp-clang-root=A:\al82dev-clang --no-restore -O0 --output-format=json > "%DEST%\build.log" 2>&1
if errorlevel 1 (type "%DEST%\build.log" & exit /b 1)
findstr /L /C:"\"linker\":\"LLD\"" "%DEST%\build.log" >nul
if errorlevel 1 (type "%DEST%\build.log" & exit /b 1)
"%DEST%\bin\Debug\TransitiveConsumer.exe"
if errorlevel 1 exit /b 1
echo TLS transitivo precompilado correcto.
exit /b 0
