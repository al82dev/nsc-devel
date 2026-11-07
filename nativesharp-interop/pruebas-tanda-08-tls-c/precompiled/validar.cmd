@echo off
setlocal
set "DEST=A:\nsc-interop-build\tls-abi\precompiled"
set "PATH=A:\al82dev-clang\bin;A:\nsc-interop-build\tanda1-ON\bin;%PATH%"
set "NSC_LINKER=A:\al82dev-clang\bin\lld-link.exe"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
if not exist "%DEST%" mkdir "%DEST%"
if errorlevel 1 exit /b 1
set "TMP=%DEST%"
set "TEMP=%DEST%"
for %%F in (Producer.ns Producer.nsproj Consumer.ns Consumer.nsproj ConsumerNoHeader.ns ConsumerNoHeader.nsproj) do (
  copy /y "%~dp0%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
for %%F in (Api.c Api.h Crt.h Probe.c) do (
  copy /y "%~dp0..\static-global\%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 "%DEST%\Api.c" -o "%DEST%\api.obj"
if errorlevel 1 exit /b 1
llvm-lib.exe /nologo /out:"%DEST%\api.lib" "%DEST%\api.obj"
if errorlevel 1 exit /b 1
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 "%DEST%\Probe.c" -o "%DEST%\probe.obj"
if errorlevel 1 exit /b 1
lld-link.exe /nologo /nodefaultlib /subsystem:console /entry:entry /out:"%DEST%\probe.exe" "%DEST%\probe.obj" "%DEST%\api.lib" "A:\vc2026-runtime\lib\x64\release\libcmt.lib" "A:\vc2026-runtime\lib\x64\release\libcpmt.lib" "A:\vc2026-runtime\lib\x64\release\libvcruntime.lib" "A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\ucrt\x64\libucrt.lib" "A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\um\x64\kernel32.lib"
if errorlevel 1 exit /b 1
"%DEST%\probe.exe"
if errorlevel 1 exit /b 1
echo Control C TLS precompilado correcto.
for %%O in (0 1 2 3) do (
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-lib "%DEST%\Producer.nsproj" -O%%O --no-artifact-cache -o "%DEST%\TlsProducer.lib"
  if errorlevel 1 exit /b 1
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Consumer.nsproj" -O%%O --no-artifact-cache -o "%DEST%\consumer-O%%O.exe"
  if errorlevel 1 exit /b 1
  "%DEST%\consumer-O%%O.exe"
  if errorlevel 1 exit /b 1
  call :without_header %%O
  if errorlevel 1 exit /b 1
  echo TLS C precompilado O%%O correcto.
)
exit /b 0

:without_header
if exist "%DEST%\Api.h.hidden" exit /b 1
move /y "%DEST%\Api.h" "%DEST%\Api.h.hidden" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\ConsumerNoHeader.nsproj" -O%~1 --no-artifact-cache -o "%DEST%\consumer-no-header-O%~1.exe"
set "TASK_HEADERLESS_RESULT=%ERRORLEVEL%"
move /y "%DEST%\Api.h.hidden" "%DEST%\Api.h" >nul
if errorlevel 1 exit /b 1
if not "%TASK_HEADERLESS_RESULT%"=="0" exit /b 1
"%DEST%\consumer-no-header-O%~1.exe"
if errorlevel 1 exit /b 1
exit /b 0
