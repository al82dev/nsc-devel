@echo off
setlocal
set "PATH=A:\nsc-interop-build\tanda1-ON\bin;A:\al82dev-clang\bin;%PATH%"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
set "NSC_LINKER=A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
for %%O in (0 1 2 3) do call :one %%O || exit /b 1
echo Estados independientes validados de O0 a O3.
exit /b 0
:one
set "DEST=A:\nsc-interop-build\t08-static-dos-mutables-O%1"
if not exist "%DEST%\a" mkdir "%DEST%\a"
if not exist "%DEST%\b" mkdir "%DEST%\b"
for %%D in (a b) do for %%F in (same.h Producer.ns Producer.nsproj) do (
  copy /y "%~dp0%%D\%%F" "%DEST%\%%D\%%F" >nul
  if errorlevel 1 exit /b 1
)
for %%F in (Consumer.ns Consumer.nsproj) do (
  copy /y "%~dp0%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
pushd "%DEST%" || exit /b 1
for %%D in (a b) do (
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-lib "%%D\Producer.nsproj" -O%1 --no-artifact-cache -o "%%D\InlineProducer%%D.lib" > "%%D\build.log" 2>&1
  if errorlevel 1 (type "%%D\build.log" & popd & exit /b 1)
)
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe Consumer.nsproj -O%1 --no-artifact-cache -o Consumer.exe > Consumer.log 2>&1
if errorlevel 1 (type Consumer.log & popd & exit /b 1)
Consumer.exe
if errorlevel 1 (popd & exit /b 1)
popd
exit /b 0
