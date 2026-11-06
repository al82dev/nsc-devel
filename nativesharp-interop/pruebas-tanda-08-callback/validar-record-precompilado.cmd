@echo off
setlocal
set "DEST=A:\nsc-interop-build\tanda08-callback-precompilado"
set "PATH=A:\al82dev-clang\bin;A:\nsc-interop-build\tanda1-ON\bin;%PATH%"
set "NSC_LINKER=A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
if not exist "%DEST%" mkdir "%DEST%"
if errorlevel 1 exit /b 1
set "TMP=%DEST%"
set "TEMP=%DEST%"
for %%F in (Api.h Api.c RecordProducer.ns RecordProducer.nsproj RecordConsumer.ns RecordConsumer.nsproj RecordConsumerNoHeader.ns RecordConsumerNoHeader.nsproj) do (
  copy /y "%~dp0%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 "%DEST%\Api.c" -o "%DEST%\api.obj"
if errorlevel 1 exit /b 1
llvm-lib.exe /nologo /out:"%DEST%\api.lib" "%DEST%\api.obj"
if errorlevel 1 exit /b 1
set "LEVELS=0 1 2 3"
if /I "%~1"=="--diagnostico" set "LEVELS=0"
for %%O in (%LEVELS%) do (
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-lib "%DEST%\RecordProducer.nsproj" -O%%O --no-artifact-cache -o "%DEST%\CRecordCallbackProducer.lib"
  if errorlevel 1 exit /b 1
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\RecordConsumer.nsproj" -O%%O --no-artifact-cache -o "%DEST%\consumer-O%%O.exe"
  if errorlevel 1 exit /b 1
  "%DEST%\consumer-O%%O.exe"
  if errorlevel 1 exit /b 1
  call :without_header %%O
  if errorlevel 1 exit /b 1
  echo Callback C con record precompilado O%%O: correcto.
)
exit /b 0

:without_header
if exist "%DEST%\Api.h.hidden" exit /b 1
move /y "%DEST%\Api.h" "%DEST%\Api.h.hidden" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\RecordConsumerNoHeader.nsproj" -O%~1 --no-artifact-cache -o "%DEST%\consumer-no-header-O%~1.exe"
set "TASK_HEADERLESS_RESULT=%ERRORLEVEL%"
move /y "%DEST%\Api.h.hidden" "%DEST%\Api.h" >nul
if errorlevel 1 exit /b 1
if not "%TASK_HEADERLESS_RESULT%"=="0" exit /b 1
"%DEST%\consumer-no-header-O%~1.exe"
if errorlevel 1 exit /b 1
exit /b 0
