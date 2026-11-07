@echo off
setlocal
set "TASK_META_ROOT=A:\nsc-interop-build\tanda08-meta-c"
set "PATH=A:\al82dev-clang\bin;A:\nsc-interop-build\tanda1-ON\bin;%PATH%"
set "NSC_LINKER=A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
for %%O in (0 1 2 3) do (
  call :run %%O
  if errorlevel 1 exit /b 1
)
for %%O in (0 1 2 3) do (
  call :run %%O value
  if errorlevel 1 exit /b 1
)
for %%O in (0 1 2 3) do (
  call :run %%O inferred
  if errorlevel 1 exit /b 1
)
for %%O in (0 1 2 3) do (
  call :run %%O independent
  if errorlevel 1 exit /b 1
)
for %%O in (0 1 2 3) do (
  call :run %%O shared
  if errorlevel 1 exit /b 1
)
for %%O in (0 1 2 3) do (
  call :run %%O sharedvalue
  if errorlevel 1 exit /b 1
)
for %%O in (0 1 2 3) do (
  call :run %%O sharedenum
  if errorlevel 1 exit /b 1
)
for %%O in (0 1 2 3) do (
  call :run %%O enumpromote
  if errorlevel 1 exit /b 1
)
for %%O in (0 1 2 3) do (
  call :run %%O sharedunion
  if errorlevel 1 exit /b 1
)
for %%O in (0 1 2 3) do (
  call :run %%O sharedcallback
  if errorlevel 1 exit /b 1
)
call :run 0 conflict
if errorlevel 1 exit /b 1
echo Public C type metadata consumer compiled and ran in O0 through O3.
exit /b 0

:run
set "DEST=%TASK_META_ROOT%\O%~1-%~2"
if not exist "%DEST%" mkdir "%DEST%"
if errorlevel 1 exit /b 1
for %%F in (Api.h Api.c Conflict.h Independent.h Independent.c Producer.nsproj Producer.ns Consumer.nsproj Consumer.ns Inferred.nsproj Inferred.ns Independent.nsproj Independent.ns Shared.nsproj Shared.ns SharedValue.nsproj SharedValue.ns SharedEnum.nsproj SharedEnum.ns SharedEnumPromote.nsproj SharedEnumPromote.ns ProducerUnion.ns SharedUnion.ns SharedUnion.nsproj ProducerCallback.ns SharedCallback.ns SharedCallback.nsproj) do (
  copy /y "%~dp0%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
if /I "%~2"=="value" (
  copy /y "%~dp0ProducerValue.ns" "%DEST%\Producer.ns" >nul
  if errorlevel 1 exit /b 1
  copy /y "%~dp0ConsumerValue.ns" "%DEST%\Consumer.ns" >nul
  if errorlevel 1 exit /b 1
)
if /I "%~2"=="sharedvalue" (
  copy /y "%~dp0ProducerValue.ns" "%DEST%\Producer.ns" >nul
  if errorlevel 1 exit /b 1
)
if /I "%~2"=="sharedunion" (
  copy /y "%~dp0ProducerUnion.ns" "%DEST%\Producer.ns" >nul
  if errorlevel 1 exit /b 1
)
if /I "%~2"=="sharedcallback" (
  copy /y "%~dp0ProducerCallback.ns" "%DEST%\Producer.ns" >nul
  if errorlevel 1 exit /b 1
)
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 -O%~1 "%DEST%\Api.c" -o "%DEST%\api.obj"
if errorlevel 1 exit /b 1
llvm-lib.exe /nologo /out:"%DEST%\api.lib" "%DEST%\api.obj"
if errorlevel 1 exit /b 1
if /I "%~2"=="inferred" (
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Inferred.nsproj" -O%~1 --no-artifact-cache -o "%DEST%\Inferred.exe"
  if errorlevel 1 exit /b 1
  "%DEST%\Inferred.exe"
  if errorlevel 1 exit /b 1
  echo Inferred C record consumer passed O%~1.
  exit /b 0
)
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-lib "%DEST%\Producer.nsproj" -O%~1 --no-artifact-cache -o "%DEST%\CPublishedProducer.lib"
if errorlevel 1 exit /b 1
if "%~2"=="" (
  A:\nsc-interop-build\tanda1-ON\bin\nsc_cpp_session_tests.exe --verify-published-c-usr "%DEST%\CPublishedProducer.nsmeta" "%DEST%\Api.h" A:\al82dev-clang\lib\clang\23 A:\al82dev-clang\bin\clang.exe
  if errorlevel 1 exit /b 1
)
if /I "%~2"=="sharedcallback" (
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\SharedCallback.nsproj" -O%~1 --no-artifact-cache -o "%DEST%\SharedCallback.exe"
  if errorlevel 1 exit /b 1
  "%DEST%\SharedCallback.exe"
  if errorlevel 1 exit /b 1
  echo Shared C callback metadata passed O%~1.
  exit /b 0
)
if /I "%~2"=="sharedunion" (
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\SharedUnion.nsproj" -O%~1 --no-artifact-cache -o "%DEST%\SharedUnion.exe"
  if errorlevel 1 exit /b 1
  "%DEST%\SharedUnion.exe"
  if errorlevel 1 exit /b 1
  echo Shared C union metadata passed O%~1.
  exit /b 0
)
if /I "%~2"=="sharedenum" (
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\SharedEnum.nsproj" -O%~1 --no-artifact-cache -o "%DEST%\SharedEnum.exe"
  if errorlevel 1 exit /b 1
  "%DEST%\SharedEnum.exe"
  if errorlevel 1 exit /b 1
  echo Shared C enum passed O%~1.
  exit /b 0
)
if /I "%~2"=="enumpromote" (
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\SharedEnumPromote.nsproj" -O%~1 --no-artifact-cache -o "%DEST%\SharedEnumPromote.exe"
  if errorlevel 1 exit /b 1
  "%DEST%\SharedEnumPromote.exe"
  if errorlevel 1 exit /b 1
  echo C enum integer comparison passed O%~1.
  exit /b 0
)
if /I "%~2"=="independent" (
  clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 -O%~1 "%DEST%\Independent.c" -o "%DEST%\independent.obj"
  if errorlevel 1 exit /b 1
  llvm-lib.exe /nologo /out:"%DEST%\independent.lib" "%DEST%\independent.obj"
  if errorlevel 1 exit /b 1
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Independent.nsproj" -O%~1 --no-artifact-cache -o "%DEST%\Independent.exe"
  if errorlevel 1 exit /b 1
  "%DEST%\Independent.exe"
  if errorlevel 1 exit /b 1
  echo Independent C header consumer passed O%~1.
  exit /b 0
)
if /I "%~2"=="conflict" (
  copy /y "%DEST%\Conflict.h" "%DEST%\Api.h" >nul
  if errorlevel 1 exit /b 1
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Shared.nsproj" -O%~1 --no-artifact-cache -o "%DEST%\Conflict.exe" > "%DEST%\conflict.log" 2>&1
  if not errorlevel 1 exit /b 1
  findstr /L /C:"C/C++ type alias conflicts" "%DEST%\conflict.log" >nul
  if errorlevel 1 exit /b 1
  echo Changed C header was rejected.
  exit /b 0
)
if /I "%~2"=="shared" (
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Shared.nsproj" -O%~1 --no-artifact-cache -o "%DEST%\Shared.exe"
  if errorlevel 1 exit /b 1
  "%DEST%\Shared.exe"
  if errorlevel 1 exit /b 1
  echo Shared C header consumer passed O%~1.
  exit /b 0
)
if /I "%~2"=="sharedvalue" (
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\SharedValue.nsproj" -O%~1 --no-artifact-cache -o "%DEST%\SharedValue.exe"
  if errorlevel 1 exit /b 1
  "%DEST%\SharedValue.exe"
  if errorlevel 1 exit /b 1
  echo Shared C records by value passed O%~1.
  exit /b 0
)
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Consumer.nsproj" -O%~1 --no-artifact-cache -o "%DEST%\Consumer.exe"
if errorlevel 1 exit /b 1
"%DEST%\Consumer.exe"
if errorlevel 1 exit /b 1
echo Public C type metadata consumer passed O%~1.
exit /b 0
