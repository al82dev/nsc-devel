@echo off
setlocal

if "%~1"=="" (
    set "NSC=nsc.exe"
) else (
    set "NSC=%~1"
)

set "ROOT=%~dp0"
set "OUT=%ROOT%out"

if exist "%OUT%" rmdir /s /q "%OUT%"
mkdir "%OUT%" || exit /b 1

"%NSC%" -O2 --emit-project-oop-exe "%ROOT%Stage50Ownership.nsproj" -o "%OUT%\Stage50Ownership.exe"
if errorlevel 1 exit /b 1

"%OUT%\Stage50Ownership.exe"
if errorlevel 1 exit /b 1

echo Stage 50 ownership example: PASS
exit /b 0
