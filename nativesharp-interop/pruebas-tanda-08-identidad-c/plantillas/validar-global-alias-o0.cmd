@echo off
setlocal
set "DEST=A:\nsc-interop-build\t08-template-global-alias"
set "SRC=%~dp0"
set "PATH=A:\nsc-interop-build\tanda1-ON\bin;A:\al82dev-clang\bin;%PATH%"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
set "NSC_LINKER=A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
if not exist "%DEST%" mkdir "%DEST%"
for %%F in (Gen.h Gen.c Producer.ns CGenProducer.nsproj ConsumerAlias.ns ConsumerAlias.nsproj ConsumerGlobalWrite.ns ConsumerGlobalWrite.nsproj) do (
  copy /y "%SRC%\%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 -O0 "%DEST%\Gen.c" -o "%DEST%\gen.obj"
if errorlevel 1 exit /b 1
llvm-lib.exe /nologo /out:"%DEST%\gen.lib" "%DEST%\gen.obj"
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-lib "%DEST%\CGenProducer.nsproj" -O0 --no-artifact-cache -o "%DEST%\CGenProducer.lib" > "%DEST%\Producer.log" 2>&1
if errorlevel 1 (type "%DEST%\Producer.log" & exit /b 1)
del "%DEST%\Gen.h"
if exist "%DEST%\Gen.h" exit /b 1
for %%P in (ConsumerAlias ConsumerGlobalWrite) do (
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\%%P.nsproj" -O0 --no-artifact-cache -o "%DEST%\%%P.exe" > "%DEST%\%%P.log" 2>&1
  if errorlevel 1 (type "%DEST%\%%P.log" & exit /b 1)
  "%DEST%\%%P.exe"
  if errorlevel 1 exit /b 1
)
echo Global y alias C en plantilla sin encabezado: correctos.
exit /b 0
