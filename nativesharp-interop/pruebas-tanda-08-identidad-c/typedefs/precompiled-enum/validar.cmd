@echo off
setlocal
set "DEST=A:\nsc-interop-build\t08-typedef-enum-precompilado-gate"
set "PATH=A:\nsc-interop-build\tanda1-ON\bin;A:\al82dev-clang\bin;%PATH%"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
set "NSC_LINKER=A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
if not exist "%DEST%" mkdir "%DEST%"
if errorlevel 1 exit /b 1
for %%F in (x.h y.h z.h Producer.ns Consumer.ns Producer.nsproj Control.nsproj Conflict.nsproj) do (
  copy /y "%~dp0%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-lib "%DEST%\Producer.nsproj" -O0 --no-artifact-cache -o "%DEST%\EnumProducer.lib" > "%DEST%\Producer.log" 2>&1
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Control.nsproj" -O0 --no-artifact-cache -o "%DEST%\Control.exe" > "%DEST%\Control.log" 2>&1
if errorlevel 1 (type "%DEST%\Control.log" & exit /b 1)
"%DEST%\Control.exe"
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --check-project-method-bodies "%DEST%\Conflict.nsproj" > "%DEST%\Conflict.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /c:"conflicting C/C++ type alias" "%DEST%\Conflict.log" >nul
if errorlevel 1 exit /b 1
del "%DEST%\x.h"
if exist "%DEST%\x.h" exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Control.nsproj" -O0 --no-artifact-cache -o "%DEST%\Absent.exe" > "%DEST%\Absent.log" 2>&1
if errorlevel 1 (type "%DEST%\Absent.log" & exit /b 1)
"%DEST%\Absent.exe"
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --check-project-method-bodies "%DEST%\Conflict.nsproj" > "%DEST%\AbsentConflict.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /c:"conflicting C/C++ type alias" "%DEST%\AbsentConflict.log" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0x.h" "%DEST%\x.h" >nul
if errorlevel 1 exit /b 1
echo /* changed after producer */>>"%DEST%\x.h"
nsc.exe --cpp-clang-root=A:\al82dev-clang --check-project-method-bodies "%DEST%\Control.nsproj" > "%DEST%\Stale.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /c:"conflicting C/C++ type alias" "%DEST%\Stale.log" >nul
if errorlevel 1 exit /b 1
echo Typedef enum C precompilado: control y conflictos correctos.
exit /b 0
