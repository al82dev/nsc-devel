@echo off
setlocal EnableExtensions

if "%~1"=="" (
    echo Usage: build_project.cmd project.nsproj [output.exe]
    exit /b 2
)

set "PROJECT=%~f1"
if not exist "%PROJECT%" (
    echo [ERROR] Project not found: %PROJECT%
    exit /b 2
)

if "%~2"=="" (
    set "OUTPUT=%~dp1%~n1.exe"
) else (
    set "OUTPUT=%~f2"
)

rem Object file goes next to the .exe, same base name, .obj extension -- derived from OUTPUT
rem (not from PROJECT) so it always matches whatever name/dir the caller actually picked.
for %%A in ("%OUTPUT%") do set "OBJ_OUTPUT=%%~dpA%%~nA.obj"

if not defined NSC_OPT set "NSC_OPT=-O0"

call "%~dp0setup_toolchain.cmd"
if errorlevel 1 exit /b %ERRORLEVEL%
call "%~dp0prepare_libraries.cmd"
if errorlevel 1 exit /b %ERRORLEVEL%

for %%P in ("%PROJECT%") do set "PROJECT_NAME=%%~nP"
echo [BUILD] %PROJECT_NAME%
echo         compiler: %NSC_EXE%
echo         project : %PROJECT%
echo         obj     : %OBJ_OUTPUT%
echo         output  : %OUTPUT%
echo         opt     : %NSC_OPT%

"%NSC_EXE%" --emit-project-oop-obj "%PROJECT%" %NSC_OPT% -o "%OBJ_OUTPUT%"
set "RC=%ERRORLEVEL%"
if not "%RC%"=="0" (
    echo [FAILED] %PROJECT_NAME% ^(obj, exit %RC%^)
    exit /b %RC%
)

"%NSC_EXE%" --emit-project-oop-exe "%PROJECT%" %NSC_OPT% -o "%OUTPUT%"
set "RC=%ERRORLEVEL%"
if not "%RC%"=="0" (
    echo [FAILED] %PROJECT_NAME% ^(exit %RC%^)
    exit /b %RC%
)

echo [OK] %PROJECT_NAME%
exit /b 0
