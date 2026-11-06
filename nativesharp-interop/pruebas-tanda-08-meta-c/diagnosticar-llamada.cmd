@echo off
setlocal EnableDelayedExpansion
set "ROOT=A:\nsc-interop-build\tanda08-meta-c\matriz"
set "PATH=A:\al82dev-clang\bin;A:\nsc-interop-build\tanda1-ON\bin;%PATH%"
set "NSC_LINKER=A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
if not exist "%ROOT%" mkdir "%ROOT%"
if errorlevel 1 exit /b 1
for %%S in (GlobalScalar NamespaceScalar GlobalPointer NamespacePointer NamespaceNested) do (
  set "DEST=%ROOT%\%%S"
  if not exist "!DEST!" mkdir "!DEST!"
  if errorlevel 1 exit /b 1
  for %%F in (Api.h Api.c Producer.nsproj) do (
    copy /y "%~dp0%%F" "!DEST!\%%F" >nul
    if errorlevel 1 exit /b 1
  )
  copy /y "%~dp0%%S.ns" "!DEST!\Producer.ns" >nul
  if errorlevel 1 exit /b 1
  clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 "!DEST!\Api.c" -o "!DEST!\api.obj"
  if errorlevel 1 exit /b 1
  llvm-lib.exe /nologo /out:"!DEST!\api.lib" "!DEST!\api.obj"
  if errorlevel 1 exit /b 1
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-lib "!DEST!\Producer.nsproj" -O0 --no-artifact-cache -o "!DEST!\CPublishedProducer.lib" > "!DEST!\result.log" 2>&1
  echo %%S: !errorlevel!
)
echo Diagnostico completado; consultar result.log para cada caso.
exit /b 0
