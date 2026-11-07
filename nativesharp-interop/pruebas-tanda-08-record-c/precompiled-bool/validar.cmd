@echo off
setlocal
set "DEST=A:\nsc-interop-build\tanda08-bool-precompiled"
set "PATH=A:\al82dev-clang\bin;A:\nsc-interop-build\tanda1-ON\bin;%PATH%"
set "NSC_LINKER=A:\al82dev-clang\bin\lld-link.exe"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
if not exist "%DEST%" mkdir "%DEST%"
if errorlevel 1 exit /b 1
set "TMP=%DEST%"
set "TEMP=%DEST%"
for %%F in (Api.h Api.c Producer.ns Producer.nsproj Consumer.ns Consumer.nsproj) do (
  copy /y "%~dp0%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
copy /y "%~dp0..\..\pruebas-tanda-08-tls-c\static-global\Crt.h" "%DEST%\Crt.h" >nul
if errorlevel 1 exit /b 1
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 "%DEST%\Api.c" -o "%DEST%\api.obj"
if errorlevel 1 exit /b 1
llvm-lib.exe /nologo /out:"%DEST%\api.lib" "%DEST%\api.obj"
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --dump-project-oop-llvm "%DEST%\Producer.nsproj" > "%DEST%\producer.ll" 2>&1
if errorlevel 1 exit /b 1
findstr /r /c:"load atomic volatile i8.*@flag.*acquire, align 1" "%DEST%\producer.ll" >nul
if errorlevel 1 exit /b 1
findstr /r /c:"store atomic volatile i8.*@flag.*release, align 1" "%DEST%\producer.ll" >nul
if errorlevel 1 exit /b 1
for %%O in (0 1 2 3) do (
  call :run_one %%O
  if errorlevel 1 exit /b 1
  echo Global C bool precompilado sin header O%%O correcto.
)
exit /b 0

:run_one
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-lib "%DEST%\Producer.nsproj" -O%~1 --no-artifact-cache -o "%DEST%\BoolProducer.lib" > "%DEST%\producer-O%~1.log" 2>&1
if errorlevel 1 (type "%DEST%\producer-O%~1.log" & exit /b 1)
if exist "%DEST%\Api.h.hidden" exit /b 1
move /y "%DEST%\Api.h" "%DEST%\Api.h.hidden" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Consumer.nsproj" -O%~1 --no-artifact-cache -o "%DEST%\consumer-O%~1.exe" > "%DEST%\consumer-O%~1.log" 2>&1
set "TASK_HEADERLESS_RESULT=%ERRORLEVEL%"
move /y "%DEST%\Api.h.hidden" "%DEST%\Api.h" >nul
if errorlevel 1 exit /b 1
if not "%TASK_HEADERLESS_RESULT%"=="0" (type "%DEST%\consumer-O%~1.log" & exit /b 1)
"%DEST%\consumer-O%~1.exe"
if errorlevel 1 exit /b 1
exit /b 0
