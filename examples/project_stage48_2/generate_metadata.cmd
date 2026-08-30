@echo off
setlocal
if "%~1"=="" (
    echo usage: generate_metadata.cmd ^<path-to-nsc.exe^>
    exit /b 2
)
set NSC=%~1
set ROOT=%~dp0
set PROJECT=%ROOT%Library\MetadataLibrary.nsproj
set OUT=%ROOT%out
if not exist "%OUT%" mkdir "%OUT%"
"%NSC%" --emit-project-oop-lib "%PROJECT%" -O2 -o "%OUT%\MetadataLibrary.lib" || exit /b 1
"%NSC%" --verify-library-metadata "%OUT%\MetadataLibrary.nsmeta" || exit /b 1
endlocal
