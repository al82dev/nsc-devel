@echo off
setlocal
set "DEST=A:\nsc-interop-build\tanda08-atomic-precompiled-%RANDOM%-%RANDOM%"
if exist "%DEST%" exit /b 1
mkdir "%DEST%"
if errorlevel 1 exit /b 1
set "PATH=A:\nsc-interop-build\tanda1-ON\bin;A:\al82dev-clang\bin;%PATH%"
set "NSC_LINKER=A:\al82dev-clang\bin\lld-link.exe"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
set "TMP=%DEST%"
set "TEMP=%DEST%"
for %%F in (Producer.ns Producer.nsproj Consumer.ns Consumer.nsproj Dll.ns Dll.nsproj DllConsumer.ns DllConsumer.nsproj) do (
  copy /y "%~dp0%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
for %%F in (Api.h Api.c) do (
  copy /y "%~dp0..\%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
copy /y "%~dp0..\..\static-global\Crt.h" "%DEST%\Crt.h" >nul
if errorlevel 1 exit /b 1
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 "%DEST%\Api.c" -o "%DEST%\api.obj"
if errorlevel 1 exit /b 1
llvm-lib.exe /nologo /out:"%DEST%\api.lib" "%DEST%\api.obj"
if errorlevel 1 exit /b 1
mkdir "%DEST%\bin\Debug"
if errorlevel 1 exit /b 1
for %%O in (0 1 2 3) do (
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-lib "%DEST%\Producer.nsproj" -O%%O --no-artifact-cache -o "%DEST%\AtomicTlsProducer.lib" > "%DEST%\producer-O%%O.log" 2>&1
  if errorlevel 1 (type "%DEST%\producer-O%%O.log" & exit /b 1)
  move /y "%DEST%\Api.h" "%DEST%\Api.h.hidden" >nul
  if errorlevel 1 exit /b 1
  call :consumer %%O
  if errorlevel 1 (
    move /y "%DEST%\Api.h.hidden" "%DEST%\Api.h" >nul
    exit /b 1
  )
  move /y "%DEST%\Api.h.hidden" "%DEST%\Api.h" >nul
  if errorlevel 1 exit /b 1
  echo Biblioteca TLS atomica precompilada y DLL O%%O correctas.
)
exit /b 0
:consumer
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Consumer.nsproj" -O%~1 --no-artifact-cache -o "%DEST%\consumer-O%~1.exe" > "%DEST%\consumer-O%~1.log" 2>&1
if errorlevel 1 (type "%DEST%\consumer-O%~1.log" & exit /b 1)
"%DEST%\consumer-O%~1.exe"
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-dll "%DEST%\Dll.nsproj" -O%~1 --no-artifact-cache -o "%DEST%\bin\Debug\AtomicTlsDll.dll" > "%DEST%\dll-O%~1.log" 2>&1
if errorlevel 1 (type "%DEST%\dll-O%~1.log" & exit /b 1)
llvm-readobj.exe --coff-tls-directory "%DEST%\bin\Debug\AtomicTlsDll.dll" > "%DEST%\dll-pe-O%~1.log" 2>&1
if errorlevel 1 exit /b 1
findstr /L /C:"AddressOfIndex:" "%DEST%\dll-pe-O%~1.log" >nul
if errorlevel 1 (type "%DEST%\dll-pe-O%~1.log" & exit /b 1)
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\DllConsumer.nsproj" -O%~1 --no-artifact-cache -o "%DEST%\dll-consumer-O%~1.exe" > "%DEST%\dll-consumer-O%~1.log" 2>&1
if errorlevel 1 (type "%DEST%\dll-consumer-O%~1.log" & exit /b 1)
"%DEST%\dll-consumer-O%~1.exe"
if errorlevel 1 exit /b 1
exit /b 0
