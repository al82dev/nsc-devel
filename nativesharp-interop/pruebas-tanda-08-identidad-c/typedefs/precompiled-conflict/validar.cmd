@echo off
setlocal
set "DEST=A:\nsc-interop-build\t08-typedef-precompilado-gate"
set "PATH=A:\nsc-interop-build\tanda1-ON\bin;A:\al82dev-clang\bin;%PATH%"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
set "NSC_LINKER=A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
if not exist "%DEST%" mkdir "%DEST%"
if errorlevel 1 exit /b 1
for %%F in (x.h y.h z.h Producer.ns Consumer.ns Producer.nsproj Consumer.nsproj Control.nsproj SameHeader.nsproj) do (
  copy /y "%~dp0%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-lib "%DEST%\Producer.nsproj" -O0 --no-artifact-cache -o "%DEST%\TypedefProducer.lib" > "%DEST%\Producer.log" 2>&1
if errorlevel 1 exit /b 1
for %%P in (SameHeader Control) do (
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\%%P.nsproj" -O0 --no-artifact-cache -o "%DEST%\%%P.exe" > "%DEST%\%%P.log" 2>&1
  if errorlevel 1 (
    type "%DEST%\%%P.log"
    exit /b 1
  )
  "%DEST%\%%P.exe"
  if errorlevel 1 exit /b 1
)
nsc.exe --cpp-clang-root=A:\al82dev-clang --check-project-method-bodies "%DEST%\Consumer.nsproj" > "%DEST%\Consumer.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /c:"conflicting C/C++ type alias" "%DEST%\Consumer.log" >nul
if errorlevel 1 exit /b 1
del "%DEST%\x.h"
if exist "%DEST%\x.h" exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Control.nsproj" -O0 --no-artifact-cache -o "%DEST%\Absent.exe" > "%DEST%\Absent.log" 2>&1
if errorlevel 1 (type "%DEST%\Absent.log" & exit /b 1)
"%DEST%\Absent.exe"
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --check-project-method-bodies "%DEST%\Consumer.nsproj" > "%DEST%\AbsentConflict.log" 2>&1
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
echo Typedef C precompilado: controles y conflicto correctos.
exit /b 0
