@echo off
setlocal EnableExtensions

if "%~1"=="" (
    set "NSC=C:\native-sharp-3.0\nsc.exe"
) else (
    set "NSC=%~1"
)

where msbuild >nul 2>nul || (
    echo error: MSBuild is required. Run this from a Visual Studio x64 Native Tools Command Prompt.
    exit /b 1
)
where dotnet >nul 2>nul || (
    echo error: .NET 9 SDK is required.
    exit /b 1
)

set "ROOT=%~dp0"
set "OUT=%ROOT%out"
set "NATIVE=%OUT%\native"
set "BRIDGE=%OUT%\bridge"
set "HOST=%OUT%\host"

if exist "%OUT%" rmdir /s /q "%OUT%"
mkdir "%NATIVE%" || exit /b 1
mkdir "%BRIDGE%" || exit /b 1
mkdir "%HOST%" || exit /b 1

"%NSC%" -O2 --emit-project-oop-dll "%ROOT%Native\Stage48_6HostApi.nsproj" -o "%NATIVE%\Stage48_6HostApi.dll" || exit /b 1
"%NSC%" --verify-library-metadata "%NATIVE%\Stage48_6HostApi.nsmeta" || exit /b 1

msbuild "%ROOT%Bridge\Stage48_6CppCliBridge.vcxproj" /m /t:Build /p:Configuration=Release /p:Platform=x64 /p:NativeSharpProductDir="%NATIVE%" /p:OutDir="%BRIDGE%\" || exit /b 1

dotnet build "%ROOT%Host\Stage48_6ManagedHost.csproj" -c Release -p:Platform=x64 -p:BridgePath="%BRIDGE%\Stage48_6CppCliBridge.dll" -o "%HOST%" || exit /b 1

copy /y "%NATIVE%\Stage48_6HostApi.dll" "%HOST%\Stage48_6HostApi.dll" >nul || exit /b 1
copy /y "%BRIDGE%\Stage48_6CppCliBridge.dll" "%HOST%\Stage48_6CppCliBridge.dll" >nul || exit /b 1

"%HOST%\Stage48_6ManagedHost.exe"
set "RESULT=%ERRORLEVEL%"
if not "%RESULT%"=="0" exit /b %RESULT%

echo Stage48.6 C++/CLI end-to-end host: PASS
exit /b 0
