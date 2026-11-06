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
echo Public C type metadata consumer compiled and ran in O0 through O3.
exit /b 0

:run
set "DEST=%TASK_META_ROOT%\O%~1"
if not exist "%DEST%" mkdir "%DEST%"
if errorlevel 1 exit /b 1
for %%F in (Api.h Api.c Producer.nsproj Producer.ns Consumer.nsproj Consumer.ns) do (
  copy /y "%~dp0%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 -O%~1 "%DEST%\Api.c" -o "%DEST%\api.obj"
if errorlevel 1 exit /b 1
llvm-lib.exe /nologo /out:"%DEST%\api.lib" "%DEST%\api.obj"
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-lib "%DEST%\Producer.nsproj" -O%~1 --no-artifact-cache -o "%DEST%\CPublishedProducer.lib"
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Consumer.nsproj" -O%~1 --no-artifact-cache -o "%DEST%\Consumer.exe"
if errorlevel 1 exit /b 1
"%DEST%\Consumer.exe"
if errorlevel 1 exit /b 1
echo Public C type metadata consumer passed O%~1.
exit /b 0
