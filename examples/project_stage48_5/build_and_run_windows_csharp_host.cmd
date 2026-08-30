@echo off
setlocal

if "%~1"=="" (
    set "NSC=nsc.exe"
) else (
    set "NSC=%~1"
)

where dotnet >nul 2>nul
if errorlevel 1 (
    echo ERROR: dotnet SDK is required.
    exit /b 1
)

set "ROOT=%~dp0"
set "SHARED=%ROOT%Shared"
set "HOST=%ROOT%Host"
set "OUT=%ROOT%out"

if exist "%OUT%" rmdir /s /q "%OUT%"
mkdir "%OUT%" || exit /b 1

"%NSC%" -O2 --emit-project-oop-dll "%SHARED%\Stage48_5HostApi.nsproj" -o "%OUT%\Stage48_5HostApi.dll"
if errorlevel 1 exit /b 1

"%NSC%" --verify-library-metadata "%OUT%\Stage48_5HostApi.nsmeta"
if errorlevel 1 exit /b 1

dotnet build "%HOST%\Stage48_5CSharpHost.csproj" -c Release --nologo
if errorlevel 1 exit /b 1

copy /y "%OUT%\Stage48_5HostApi.dll" "%HOST%\bin\Release\net9.0\Stage48_5HostApi.dll" >nul
if errorlevel 1 exit /b 1

"%HOST%\bin\Release\net9.0\Stage48_5CSharpHost.exe"
if errorlevel 1 exit /b 1

echo Stage 48.5 Windows x64 C# host integration: PASS
exit /b 0
