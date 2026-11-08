@echo off
setlocal
set "DEST=A:\nsc-interop-build\t08-typedef-enum-gate"
set "PATH=A:\nsc-interop-build\tanda1-ON\bin;A:\al82dev-clang\bin;%PATH%"
if not exist "%DEST%" mkdir "%DEST%"
if errorlevel 1 exit /b 1
for %%F in (x.h y.h same-x.h same-y.h Test.ns Same.ns Control.nsproj Test.nsproj Same.nsproj) do (
  copy /y "%~dp0%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
nsc.exe --cpp-clang-root=A:\al82dev-clang --check-project-method-bodies "%DEST%\Control.nsproj" > "%DEST%\Control.log" 2>&1
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --check-project-method-bodies "%DEST%\Test.nsproj" > "%DEST%\Test.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /c:"conflicting C/C++ type alias" "%DEST%\Test.log" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --check-project-method-bodies "%DEST%\Same.nsproj" > "%DEST%\Same.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /c:"conflicting C enum declarations" "%DEST%\Same.log" >nul
if errorlevel 1 exit /b 1
echo Typedef C enum: control y dos conflictos correctos.
exit /b 0
