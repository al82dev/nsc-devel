@echo off
setlocal
set "DEST=A:\nsc-interop-build\t08-homonimos-global-gate"
set "PATH=A:\nsc-interop-build\tanda1-ON\bin;A:\al82dev-clang\bin;%PATH%"
if not exist "%DEST%" mkdir "%DEST%"
if errorlevel 1 exit /b 1
for %%D in (dirA dirB dirC) do (
  if not exist "%DEST%\%%D" mkdir "%DEST%\%%D"
  copy /y "%~dp0%%D\same.h" "%DEST%\%%D\same.h" >nul
  if errorlevel 1 exit /b 1
)
for %%F in (GlobalStatic.ns GlobalStaticA.nsproj GlobalStaticAB.nsproj GlobalStaticAC.nsproj) do (
  copy /y "%~dp0%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
nsc.exe --cpp-clang-root=A:\al82dev-clang --check-project-method-bodies "%DEST%\GlobalStaticA.nsproj" > "%DEST%\A.log" 2>&1
if errorlevel 1 exit /b 1
for %%P in (AB AC) do (
  nsc.exe --cpp-clang-root=A:\al82dev-clang --check-project-method-bodies "%DEST%\GlobalStatic%%P.nsproj" > "%DEST%\%%P.log" 2>&1
  if not errorlevel 1 exit /b 1
  findstr /c:"ambiguous C internal global origins" "%DEST%\%%P.log" >nul
  if errorlevel 1 exit /b 1
)
echo Global static C: control unico y dos ambiguedades correctos.
exit /b 0
