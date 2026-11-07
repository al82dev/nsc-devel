@echo off
setlocal EnableDelayedExpansion
set "DEST=A:\nsc-interop-build\tanda08-dll-emision-auto\run-%RANDOM%-%RANDOM%"
if exist "%DEST%" exit /b 1
mkdir "%DEST%"
if errorlevel 1 exit /b 1
set "PATH=A:\nsc-interop-build\tanda1-ON\bin;A:\al82dev-clang\bin;A:\nsc-interop-build\cpp-fixtures\command-cpp;%PATH%"
set "NSC_LINKER="
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
set "TMP=%DEST%"
set "TEMP=%DEST%"
for %%F in (Api.c Api.h Crt.h) do (
  copy /y "%~dp0..\static-global\%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
copy /y "%~dp0..\precompiled\Producer.ns" "%DEST%\Producer.ns" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0SharedProducer.nsproj" "%DEST%\SharedProducer.nsproj" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0SharedConsumer.nsproj" "%DEST%\SharedConsumer.nsproj" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0..\precompiled\ConsumerNoHeader.ns" "%DEST%\ConsumerNoHeader.ns" >nul
if errorlevel 1 exit /b 1
mkdir "%DEST%\bin\Debug"
if errorlevel 1 exit /b 1
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 "%DEST%\Api.c" -o "%DEST%\api.obj"
if errorlevel 1 exit /b 1
llvm-lib.exe /nologo /out:"%DEST%\api.lib" "%DEST%\api.obj"
if errorlevel 1 exit /b 1
for %%O in (0 1 2 3) do (
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-dll "%DEST%\SharedProducer.nsproj" --no-artifact-cache -O%%O -o "%DEST%\bin\Debug\TlsSharedProducer.dll" > "%DEST%\auto-%%O.log" 2>&1
  if not "!ERRORLEVEL!"=="0" (type "%DEST%\auto-%%O.log" & exit /b 1)
  llvm-readobj.exe --coff-tls-directory "%DEST%\bin\Debug\TlsSharedProducer.dll" > "%DEST%\pe-%%O.log"
  if not "!ERRORLEVEL!"=="0" exit /b 1
  findstr /L /C:"AddressOfIndex:" "%DEST%\pe-%%O.log" >nul
  if errorlevel 1 (type "%DEST%\pe-%%O.log" & exit /b 1)
  python "%~dp0validar-dll-emision-auto.py" "%DEST%\bin\Debug\TlsSharedProducer.dll"
  if not "!ERRORLEVEL!"=="0" exit /b 1
  nsc.exe --emit-project-oop-exe "%DEST%\SharedConsumer.nsproj" --no-artifact-cache -O%%O -o "%DEST%\consumer-%%O.exe" > "%DEST%\consumer-%%O.log" 2>&1
  if not "!ERRORLEVEL!"=="0" (type "%DEST%\consumer-%%O.log" & exit /b 1)
  "%DEST%\consumer-%%O.exe"
  if not "!ERRORLEVEL!"=="0" exit /b 1
)
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-dll "%DEST%\SharedProducer.nsproj" --nsc-linker-selection=NSLink --no-artifact-cache -O0 -o "%DEST%\explicit.dll" > "%DEST%\explicit.log" 2>&1
if not "%ERRORLEVEL%"=="0" (type "%DEST%\explicit.log" & exit /b 1)
llvm-readobj.exe --coff-tls-directory "%DEST%\explicit.dll" > "%DEST%\explicit-pe.log"
if errorlevel 1 exit /b 1
findstr /L /C:"AddressOfIndex:" "%DEST%\explicit-pe.log" >nul
if not errorlevel 1 exit /b 1
set "NSC_LINKER=A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-dll "%DEST%\SharedProducer.nsproj" --no-artifact-cache -O0 -o "%DEST%\env.dll" > "%DEST%\env.log" 2>&1
if not "%ERRORLEVEL%"=="0" (type "%DEST%\env.log" & exit /b 1)
llvm-readobj.exe --coff-tls-directory "%DEST%\env.dll" > "%DEST%\env-pe.log"
if errorlevel 1 exit /b 1
findstr /L /C:"AddressOfIndex:" "%DEST%\env-pe.log" >nul
if not errorlevel 1 exit /b 1
echo Emision DLL directa TLS y elecciones explicitas correctas.
exit /b 0
