@echo off
setlocal EnableExtensions DisableDelayedExpansion

set "NSC_DEPLOY_BUILD="
set "NSC_DEPLOY_PREFIX="

:parse_arguments
if "%~1"=="" goto validate_arguments
if /I "%~1"=="--build" goto parse_build
if /I "%~1"=="--prefix" goto parse_prefix
>&2 echo Unknown deploy argument: %~1
exit /b 2

:parse_build
if defined NSC_DEPLOY_BUILD goto duplicate_build
if "%~2"=="" goto missing_build
set "NSC_DEPLOY_BUILD=%~2"
shift
shift
goto parse_arguments

:parse_prefix
if defined NSC_DEPLOY_PREFIX goto duplicate_prefix
if "%~2"=="" goto missing_prefix
set "NSC_DEPLOY_PREFIX=%~2"
shift
shift
goto parse_arguments

:validate_arguments
if not defined NSC_DEPLOY_BUILD goto missing_build
if not defined NSC_DEPLOY_PREFIX goto missing_prefix

rem Normalize caller-selected paths before checking destructive boundaries.
for %%I in ("%NSC_DEPLOY_BUILD%") do set "NSC_DEPLOY_BUILD=%%~fI"
for %%I in ("%NSC_DEPLOY_PREFIX%") do set "NSC_DEPLOY_PREFIX=%%~fI"
for %%I in ("%NSC_DEPLOY_PREFIX%") do set "NSC_DEPLOY_PREFIX_ROOT=%%~dI\"

if /I "%NSC_DEPLOY_PREFIX%"=="%NSC_DEPLOY_PREFIX_ROOT%" goto root_prefix
if /I "%NSC_DEPLOY_BUILD%"=="%NSC_DEPLOY_PREFIX%" goto overlapping_paths

where cmake >nul 2>&1
if errorlevel 1 goto missing_cmake

cmake --install "%NSC_DEPLOY_BUILD%" --prefix "%NSC_DEPLOY_PREFIX%"
exit /b %ERRORLEVEL%

:duplicate_build
>&2 echo --build may be specified only once.
exit /b 2

:duplicate_prefix
>&2 echo --prefix may be specified only once.
exit /b 2

:missing_build
>&2 echo --build requires one non-empty path.
exit /b 2

:missing_prefix
>&2 echo --prefix requires one non-empty path.
exit /b 2

:root_prefix
>&2 echo Refusing to deploy to a drive root: %NSC_DEPLOY_PREFIX%
exit /b 2

:overlapping_paths
>&2 echo Build and prefix paths must differ.
exit /b 2

:missing_cmake
>&2 echo CMake was not found on PATH.
exit /b 3
