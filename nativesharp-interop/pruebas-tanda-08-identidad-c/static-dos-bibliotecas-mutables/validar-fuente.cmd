@echo off
setlocal
set "DEST=A:\nsc-interop-build\t08-static-proyectos-fuente-gate"
set "PATH=A:\nsc-interop-build\tanda1-ON\bin;A:\al82dev-clang\bin;%PATH%"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
set "NSC_LINKER=A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
for %%D in (a b) do (
  if not exist "%DEST%\%%D" mkdir "%DEST%\%%D"
  for %%F in (same.h Producer.ns Producer.nsproj) do (
    copy /y "%~dp0%%D\%%F" "%DEST%\%%D\%%F" >nul
    if errorlevel 1 exit /b 1
  )
)
for %%F in (Consumer.ns Consumer-source.nsproj) do (
  copy /y "%~dp0%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
pushd "%DEST%" || exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe Consumer-source.nsproj -O0 --no-artifact-cache -o Consumer.exe > Consumer.log 2>&1
if errorlevel 1 (type Consumer.log & popd & exit /b 1)
Consumer.exe
if errorlevel 1 (popd & exit /b 1)
popd
echo Proyectos fuente mantienen globales internas independientes.
