@echo off
setlocal
set "DEST=A:\nsc-interop-build\tanda08-atomic-global-precompiled-%RANDOM%-%RANDOM%"
if exist "%DEST%" exit /b 1
mkdir "%DEST%"
if errorlevel 1 goto :failure
set "PATH=A:\al82dev-clang\bin;A:\nsc-interop-build\tanda1-ON\bin;%PATH%"
set "NSC_LINKER=A:\al82dev-clang\bin\lld-link.exe"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
set "TMP=%DEST%"
set "TEMP=%DEST%"
for %%F in (Api.h Api.c Producer.ns Producer.nsproj Consumer.ns Consumer.nsproj Dll.ns Dll.nsproj DllConsumer.ns DllConsumer.nsproj) do (
  copy /y "%~dp0%%F" "%DEST%\%%F" >nul
  if errorlevel 1 goto :failure
)
mkdir "%DEST%\bin\Debug"
if errorlevel 1 goto :failure
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 "%DEST%\Api.c" -o "%DEST%\api.obj"
if errorlevel 1 goto :failure
llvm-lib.exe /nologo /out:"%DEST%\api.lib" "%DEST%\api.obj"
if errorlevel 1 goto :failure
for %%O in (0 1 2 3) do (
  call :run_one %%O
  if errorlevel 1 goto :failure
  echo Global atomico precompilado y DLL O%%O correctos.
)
exit /b 0

:run_one
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-lib "%DEST%\Producer.nsproj" -O%~1 --no-artifact-cache -o "%DEST%\AtomicGlobalProducer.lib" > "%DEST%\producer-O%~1.log" 2>&1
if errorlevel 1 goto :failure
move /y "%DEST%\Api.h" "%DEST%\Api.h.hidden" >nul
if errorlevel 1 goto :failure
call :consumer %~1
set "TASK_RESULT=%ERRORLEVEL%"
move /y "%DEST%\Api.h.hidden" "%DEST%\Api.h" >nul
if errorlevel 1 goto :failure
if not "%TASK_RESULT%"=="0" goto :failure
exit /b 0

:consumer
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Consumer.nsproj" -O%~1 --no-artifact-cache -o "%DEST%\consumer-O%~1.exe" > "%DEST%\consumer-O%~1.log" 2>&1
if errorlevel 1 goto :failure
"%DEST%\consumer-O%~1.exe"
if errorlevel 1 goto :failure
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-dll "%DEST%\Dll.nsproj" -O%~1 --no-artifact-cache -o "%DEST%\bin\Debug\AtomicGlobalDll.dll" > "%DEST%\dll-O%~1.log" 2>&1
if errorlevel 1 goto :failure
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\DllConsumer.nsproj" -O%~1 --no-artifact-cache -o "%DEST%\dll-consumer-O%~1.exe" > "%DEST%\dll-consumer-O%~1.log" 2>&1
if errorlevel 1 goto :failure
"%DEST%\dll-consumer-O%~1.exe"
if errorlevel 1 goto :failure
exit /b 0
:failure
type "%DEST%\*.log"
exit /b 1
