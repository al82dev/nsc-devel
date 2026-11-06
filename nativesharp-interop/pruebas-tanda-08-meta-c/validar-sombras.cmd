@echo off
setlocal EnableDelayedExpansion
set "ROOT=A:\nsc-interop-build\tanda08-meta-c\sombras"
set "PATH=A:\al82dev-clang\bin;A:\nsc-interop-build\tanda1-ON\bin;%PATH%"
set "NSC_LINKER=A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
if not exist "%ROOT%" mkdir "%ROOT%"
if errorlevel 1 exit /b 1
for %%S in (ShadowNamespace ShadowUsing ShadowMember NamespaceGlobalValue ShadowValueNamespace ShadowValueUsing ShadowValueLocal) do (
  set "DEST=%ROOT%\%%S"
  if not exist "!DEST!" mkdir "!DEST!"
  if errorlevel 1 exit /b 1
  for %%F in (Api.h Api.c Shadow.nsproj) do (
    copy /y "%~dp0%%F" "!DEST!\%%F" >nul
    if errorlevel 1 exit /b 1
  )
  copy /y "%~dp0%%S.ns" "!DEST!\Main.ns" >nul
  if errorlevel 1 exit /b 1
  clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 "!DEST!\Api.c" -o "!DEST!\api.obj"
  if errorlevel 1 exit /b 1
  llvm-lib.exe /nologo /out:"!DEST!\api.lib" "!DEST!\api.obj"
  if errorlevel 1 exit /b 1
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "!DEST!\Shadow.nsproj" -O0 --no-artifact-cache -o "!DEST!\program.exe"
  if errorlevel 1 exit /b 1
  "!DEST!\program.exe"
  if errorlevel 1 exit /b 1
)
echo Sombras de funciones C: correctas.
exit /b 0
