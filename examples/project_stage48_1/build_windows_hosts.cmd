@echo off
setlocal

if "%~1"=="" (
    echo Usage: build_windows_hosts.cmd C:\path\to\nsc.exe
    exit /b 2
)

set "NSC=%~1"
set "ROOT=%~dp0"
set "OUT=%ROOT%out"

if not exist "%OUT%" mkdir "%OUT%"

"%NSC%" --emit-project-oop-dll "%ROOT%Shared\Stage48_1Host.nsproj" -O2 -o "%OUT%\Stage48_1Host.dll"
if errorlevel 1 exit /b 1

clang-cl /nologo /TC /std:c11 /I"%OUT%" /c "%ROOT%Hosts\C\host.c" /Fo"%OUT%\host_c.obj"
if errorlevel 1 exit /b 1

clang-cl /nologo /TP /std:c++20 /EHs- /GR- /I"%OUT%" /c "%ROOT%Hosts\Cpp\host.cpp" /Fo"%OUT%\host_cpp.obj"
if errorlevel 1 exit /b 1

lld-link /dll /noentry /nodefaultlib /machine:x64 /out:"%OUT%\Stage48_1_C_Host.dll" /export:Stage48_1CHostProbe "%OUT%\host_c.obj" "%OUT%\Stage48_1Host.lib"
if errorlevel 1 exit /b 1

lld-link /dll /noentry /nodefaultlib /machine:x64 /out:"%OUT%\Stage48_1_CPP_Host.dll" /export:Stage48_1CppHostProbe "%OUT%\host_cpp.obj" "%OUT%\Stage48_1Host.lib"
if errorlevel 1 exit /b 1

echo Stage 48.1 C and C++ host build: PASS
exit /b 0
