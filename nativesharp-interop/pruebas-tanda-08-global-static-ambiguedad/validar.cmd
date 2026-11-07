@echo off
setlocal
set "DEST=A:\nsc-interop-build\tanda08-global-static-ambiguedad-%RANDOM%-%RANDOM%"
if exist "%DEST%" exit /b 1
mkdir "%DEST%"
if errorlevel 1 exit /b 1
set "PATH=A:\al82dev-clang\bin;A:\nsc-interop-build\tanda1-ON\bin;%PATH%"
set "TMP=%DEST%"
set "TEMP=%DEST%"
for %%F in (First.h Second.h Main.ns Project.nsproj) do (
  copy /y "%~dp0%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
nsc.exe --cpp-clang-root=A:\al82dev-clang --check-project-method-bodies "%DEST%\Project.nsproj" > "%DEST%\diagnostico.log" 2>&1
if not errorlevel 1 (
  echo NSC acepto una referencia que tiene dos origenes internos.
  exit /b 1
)
findstr /c:"ambiguous C internal global origins" "%DEST%\diagnostico.log" >nul
if errorlevel 1 goto :failure
echo Ambiguedad con tipo no proyectado detectada.
exit /b 0
:failure
type "%DEST%\diagnostico.log"
exit /b 1
