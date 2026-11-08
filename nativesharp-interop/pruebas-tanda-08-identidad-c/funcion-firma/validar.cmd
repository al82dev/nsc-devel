@echo off
setlocal
set "DEST=A:\nsc-interop-build\t08-external-firma-gate"
set "PATH=A:\nsc-interop-build\tanda1-ON\bin;A:\al82dev-clang\bin;%PATH%"
if not exist "%DEST%" mkdir "%DEST%"
if errorlevel 1 exit /b 1
for %%F in (x.h y.h z.h Main.ns Main.nsproj Control.nsproj) do (
  copy /y "%~dp0%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
nsc.exe --cpp-clang-root=A:\al82dev-clang --check-project-method-bodies "%DEST%\Control.nsproj" > "%DEST%\Control.log" 2>&1
if errorlevel 1 (type "%DEST%\Control.log" & exit /b 1)
nsc.exe --cpp-clang-root=A:\al82dev-clang --check-project-method-bodies "%DEST%\Main.nsproj" > "%DEST%\Conflict.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /c:"conflicting declarations for one Clang USR" "%DEST%\Conflict.log" >nul
if errorlevel 1 exit /b 1
echo Firmas externas C: control y conflicto correctos.
exit /b 0
