@echo off
setlocal
set "DEST=A:\nsc-interop-build\tls-abi\dll-precompiled-%RANDOM%-%RANDOM%"
if exist "%DEST%" exit /b 1
mkdir "%DEST%"
if errorlevel 1 exit /b 1
set "PATH=A:\nsc-interop-build\tanda1-ON\bin;A:\al82dev-clang\bin;A:\nsc-interop-build\cpp-fixtures\command-cpp;%PATH%"
set "NSC_LINKER="
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
set "TMP=%DEST%"
set "TEMP=%DEST%"
for %%F in (DllProducer.ns DllProducer.nsproj DllConsumer.ns DllConsumer.nsproj) do (
  copy /y "%~dp0%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
for %%F in (TlsApi.h TlsApi.c) do (
  copy /y "%~dp0..\directo\%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 "%DEST%\TlsApi.c" -o "%DEST%\api.obj"
if errorlevel 1 exit /b 1
lld-link.exe /nologo /dll /noentry /nodefaultlib /include:_tls_used /export:tls_exported,DATA /out:"%DEST%\api.dll" /implib:"%DEST%\api.lib" "%DEST%\api.obj" "A:\vc2026-runtime\lib\x64\release\libcmt.lib" "A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\um\x64\kernel32.lib"
if errorlevel 1 exit /b 1
for %%O in (0 1 2 3) do (
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-lib "%DEST%\DllProducer.nsproj" -O%%O --no-artifact-cache -o "%DEST%\DllTlsProducer.lib" > "%DEST%\producer-O%%O.log" 2>&1
  if errorlevel 1 (type "%DEST%\producer-O%%O.log" & exit /b 1)
  call :consumer_without_header %%O
  if errorlevel 1 exit /b 1
  "%DEST%\consumer-O%%O.exe"
  if errorlevel 1 exit /b 1
)
echo TLS de DLL C con productor precompilado correcto.
exit /b 0

:consumer_without_header
move /y "%DEST%\TlsApi.h" "%DEST%\TlsApi.h.hidden" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\DllConsumer.nsproj" -O%~1 --no-artifact-cache -o "%DEST%\consumer-O%~1.exe" > "%DEST%\consumer-O%~1.log" 2>&1
set "CONSUMER_RESULT=%ERRORLEVEL%"
move /y "%DEST%\TlsApi.h.hidden" "%DEST%\TlsApi.h" >nul
if errorlevel 1 exit /b 1
if not "%CONSUMER_RESULT%"=="0" (type "%DEST%\consumer-O%~1.log" & exit /b 1)
exit /b 0
