@echo off
setlocal
set "DEST=A:\nsc-interop-build\t08-plantilla-funcion"
set "PATH=A:\nsc-interop-build\tanda1-ON\bin;A:\al82dev-clang\bin;%PATH%"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
set "NSC_LINKER=A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
if not exist "%DEST%" mkdir "%DEST%"
if errorlevel 1 exit /b 1
for %%F in (Api.h Api.c Bad.h Producer.ns Producer.nsproj Consumer.ns ConsumerConflict.ns WithHeader.nsproj WithoutHeader.nsproj Conflict.nsproj Unused.nsproj) do (
  copy /y "%~dp0%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 -O0 "%DEST%\Api.c" -o "%DEST%\api.obj"
if errorlevel 1 exit /b 1
llvm-lib.exe /nologo /out:"%DEST%\api.lib" "%DEST%\api.obj"
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-lib "%DEST%\Producer.nsproj" -O0 --no-artifact-cache -o "%DEST%\CFunctionProducer.lib" > "%DEST%\Producer.log" 2>&1
if errorlevel 1 (type "%DEST%\Producer.log" & exit /b 1)
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\WithHeader.nsproj" -O0 --no-artifact-cache -o "%DEST%\WithHeader.exe" > "%DEST%\WithHeader.log" 2>&1
if errorlevel 1 (type "%DEST%\WithHeader.log" & exit /b 1)
"%DEST%\WithHeader.exe"
if errorlevel 1 exit /b 1
del "%DEST%\Api.h"
if exist "%DEST%\Api.h" exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\WithoutHeader.nsproj" -O0 --no-artifact-cache -o "%DEST%\WithoutHeader.exe" > "%DEST%\WithoutHeader.log" 2>&1
if errorlevel 1 (type "%DEST%\WithoutHeader.log" & exit /b 1)
"%DEST%\WithoutHeader.exe"
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Unused.nsproj" -O0 --no-artifact-cache -o "%DEST%\Unused.exe" > "%DEST%\Unused.log" 2>&1
if errorlevel 1 (type "%DEST%\Unused.log" & exit /b 1)
"%DEST%\Unused.exe"
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --check-project-method-bodies "%DEST%\Conflict.nsproj" > "%DEST%\Conflict.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /c:"conflicting C function declarations across precompiled library and header" "%DEST%\Conflict.log" >nul
if errorlevel 1 (type "%DEST%\Conflict.log" & exit /b 1)
echo Funcion C en plantilla: con y sin encabezado correcto.
exit /b 0
