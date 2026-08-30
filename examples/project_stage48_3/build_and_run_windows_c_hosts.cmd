@echo off
setlocal

rem Run from a Visual Studio x64 Native Tools Command Prompt.
rem Optional first argument: path to nsc.exe. Defaults to nsc on PATH.
set "NSC=nsc"
if not "%~1"=="" set "NSC=%~1"

set "ROOT=%~dp0"
set "SHARED=%ROOT%Shared"
set "STATIC=%ROOT%Static"
set "HOSTS=%ROOT%Hosts"
set "OUT=%ROOT%out"

if not exist "%OUT%" mkdir "%OUT%"

"%NSC%" --emit-project-oop-dll "%SHARED%\Stage48_3HostApi.nsproj" -O2 -o "%SHARED%\Stage48_3HostApi.dll"
if errorlevel 1 exit /b 1

"%NSC%" --emit-project-oop-lib "%STATIC%\Stage48_3HostApi.nsproj" -O2 -o "%STATIC%\Stage48_3HostApi.lib"
if errorlevel 1 exit /b 1

cl /nologo /std:c11 /W4 /WX /I"%SHARED%" "%HOSTS%\host.c" "%SHARED%\Stage48_3HostApi.lib" /Fe"%OUT%\stage48_3_c_shared.exe"
if errorlevel 1 exit /b 1
copy /y "%SHARED%\Stage48_3HostApi.dll" "%OUT%\Stage48_3HostApi.dll" >nul
"%OUT%\stage48_3_c_shared.exe"
if errorlevel 1 exit /b 1

del /q "%OUT%\Stage48_3HostApi.dll" >nul 2>nul
cl /nologo /std:c11 /W4 /WX /I"%STATIC%" "%HOSTS%\host.c" "%STATIC%\Stage48_3HostApi.lib" /Fe"%OUT%\stage48_3_c_static.exe"
if errorlevel 1 exit /b 1
"%OUT%\stage48_3_c_static.exe"
if errorlevel 1 exit /b 1

echo Stage 48.3 C hosts: PASS
exit /b 0
