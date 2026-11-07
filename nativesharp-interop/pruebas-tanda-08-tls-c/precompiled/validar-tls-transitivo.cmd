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
for %%F in (TransitiveProducer.ns TransitiveProducer.nsproj TransitiveConsumer.ns TransitiveConsumer.nsproj TransitiveDll.ns TransitiveDll.nsproj TransitiveDllConsumer.ns TransitiveDllConsumer.nsproj) do (
  copy /y "%~dp0%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
for %%F in (Api.c Api.h Crt.h) do (
  copy /y "%~dp0..\static-global\%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
for %%F in (Intermediate.ns Intermediate.nsproj IntermediateConsumer.ns IntermediateConsumer.nsproj) do (
  copy /y "%~dp0%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
for %%F in (PlainApi.h PlainApi.c PlainApiTls.c PlainProducer.ns PlainProducer.nsproj PlainProducerTls.nsproj PlainConsumer.ns PlainConsumer.nsproj) do (
  copy /y "%~dp0%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
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
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-lib "%DEST%\Intermediate.nsproj" -O0 --no-artifact-cache -o "%DEST%\Intermediate.lib" > "%DEST%\intermediate.log" 2>&1
if errorlevel 1 (type "%DEST%\intermediate.log" & exit /b 1)
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\IntermediateConsumer.nsproj" -O0 --no-artifact-cache -o "%DEST%\intermediate-consumer.exe" > "%DEST%\intermediate-consumer.log" 2>&1
if errorlevel 1 (type "%DEST%\intermediate-consumer.log" & exit /b 1)
"%DEST%\intermediate-consumer.exe"
if errorlevel 1 exit /b 1
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
for %%O in (0 1 2 3) do (
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-dll "%DEST%\TransitiveDll.nsproj" -O%%O --no-artifact-cache -o "%DEST%\bin\Debug\TransitiveDll.dll"
  if errorlevel 1 exit /b 1
  llvm-readobj.exe --coff-tls-directory "%DEST%\bin\Debug\TransitiveDll.dll" > "%DEST%\dll-pe-O%%O.log"
  if errorlevel 1 exit /b 1
  findstr /L /C:"AddressOfIndex:" "%DEST%\dll-pe-O%%O.log" >nul
  if errorlevel 1 exit /b 1
  nsc.exe --emit-project-oop-exe "%DEST%\TransitiveDllConsumer.nsproj" -O%%O --no-artifact-cache -o "%DEST%\dll-consumer-O%%O.exe"
  if errorlevel 1 exit /b 1
  "%DEST%\dll-consumer-O%%O.exe"
  if errorlevel 1 exit /b 1
)
nsc.exe build "%DEST%\TransitiveConsumer.nsproj" --cpp-clang-root=A:\al82dev-clang --no-restore --linker NSLink -O0 > "%DEST%\explicit.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /L /C:"NSLink failed" "%DEST%\explicit.log" >nul
if errorlevel 1 (type "%DEST%\explicit.log" & exit /b 1)
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\TransitiveConsumer.nsproj" --nsc-linker-selection=NSLink -O0 --no-artifact-cache -o "%DEST%\explicit.exe" > "%DEST%\direct-explicit.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /L /C:"NSLink failed" "%DEST%\direct-explicit.log" >nul
if errorlevel 1 (type "%DEST%\direct-explicit.log" & exit /b 1)
set "NSC_LINKER=A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\TransitiveConsumer.nsproj" -O0 --no-artifact-cache -o "%DEST%\env.exe" > "%DEST%\env.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /L /C:"NSLink failed" "%DEST%\env.log" >nul
if errorlevel 1 (type "%DEST%\env.log" & exit /b 1)
set "NSC_LINKER="
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 "%DEST%\PlainApi.c" -o "%DEST%\plain.obj"
if errorlevel 1 exit /b 1
llvm-lib.exe /nologo /out:"%DEST%\plain.lib" "%DEST%\plain.obj"
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-lib "%DEST%\PlainProducer.nsproj" -O0 --no-artifact-cache -o "%DEST%\PlainProducer.lib"
if errorlevel 1 exit /b 1
nsc.exe build "%DEST%\PlainConsumer.nsproj" --cpp-clang-root=A:\al82dev-clang --no-restore -O0 --output-format=json > "%DEST%\plain-build.log" 2>&1
if errorlevel 1 (type "%DEST%\plain-build.log" & exit /b 1)
findstr /L /C:"\"linker\":\"NSLink\"" "%DEST%\plain-build.log" >nul
if errorlevel 1 (type "%DEST%\plain-build.log" & exit /b 1)
"%DEST%\bin\Debug\PlainConsumer.exe"
if errorlevel 1 exit /b 1
move /y "%DEST%\plain.lib" "%DEST%\plain.lib.hidden" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\PlainConsumer.nsproj" -O0 --no-artifact-cache -o "%DEST%\missing.exe" > "%DEST%\missing.log" 2>&1
set "MISSING_RESULT=%ERRORLEVEL%"
move /y "%DEST%\plain.lib.hidden" "%DEST%\plain.lib" >nul
if errorlevel 1 exit /b 1
if "%MISSING_RESULT%"=="0" exit /b 1
findstr /L /C:"plain.lib" "%DEST%\missing.log" >nul
if errorlevel 1 (type "%DEST%\missing.log" & exit /b 1)
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 "%DEST%\PlainApiTls.c" -o "%DEST%\plain.obj"
if errorlevel 1 exit /b 1
llvm-lib.exe /nologo /out:"%DEST%\plain.lib" "%DEST%\plain.obj"
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-lib "%DEST%\PlainProducerTls.nsproj" -O0 --no-artifact-cache -o "%DEST%\PlainProducer.lib"
if errorlevel 1 exit /b 1
for %%I in (1 2) do (
  nsc.exe build "%DEST%\PlainConsumer.nsproj" --cpp-clang-root=A:\al82dev-clang --no-restore -O0 --output-format=json > "%DEST%\tls-cache-%%I.log" 2>&1
  if errorlevel 1 (type "%DEST%\tls-cache-%%I.log" & exit /b 1)
  findstr /L /C:"\"linker\":\"LLD\"" "%DEST%\tls-cache-%%I.log" >nul
  if errorlevel 1 (type "%DEST%\tls-cache-%%I.log" & exit /b 1)
  "%DEST%\bin\Debug\PlainConsumer.exe"
  if errorlevel 1 exit /b 1
  llvm-readobj.exe --coff-tls-directory "%DEST%\bin\Debug\PlainConsumer.exe" > "%DEST%\tls-cache-pe-%%I.log"
  if errorlevel 1 exit /b 1
  findstr /L /C:"AddressOfIndex:" "%DEST%\tls-cache-pe-%%I.log" >nul
  if errorlevel 1 exit /b 1
)
echo TLS transitivo precompilado correcto.
exit /b 0
