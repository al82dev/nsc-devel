@echo off
setlocal EnableExtensions

call "%~dp0setup_toolchain.cmd"
if errorlevel 1 exit /b %ERRORLEVEL%

if "%NS_USE_BUNDLED_LIB%"=="1" (
    echo [LIB] Using bundled project libraries under "%~dp0..\lib".
    endlocal & set "NS_LIB_READY=1"
    exit /b 0
)

set "PACKAGE_LIB=%~dp0..\lib"
set "CORE_SRC=%NS_LIB_ROOT%\NativeSharp.Core"
set "COLL_SRC=%NS_LIB_ROOT%\NativeSharp.Core.Collections.Generic"

if not exist "%CORE_SRC%\NativeSharp.Core.nsproj" (
    echo [ERROR] Missing installed NativeSharp.Core project:
    echo         %CORE_SRC%\NativeSharp.Core.nsproj
    exit /b 1
)
if not exist "%COLL_SRC%\NativeSharp.Core.Collections.Generic.nsproj" (
    echo [ERROR] Missing installed NativeSharp.Core.Collections.Generic project:
    echo         %COLL_SRC%\NativeSharp.Core.Collections.Generic.nsproj
    exit /b 1
)

if defined NS_LIB_READY (
    endlocal & set "NS_LIB_READY=1"
    exit /b 0
)

echo [LIB] Synchronizing installed libraries from "%NS_LIB_ROOT%"...
if exist "%PACKAGE_LIB%\NativeSharp.Core" rmdir /s /q "%PACKAGE_LIB%\NativeSharp.Core"
if exist "%PACKAGE_LIB%\NativeSharp.Core.Collections.Generic" rmdir /s /q "%PACKAGE_LIB%\NativeSharp.Core.Collections.Generic"
if not exist "%PACKAGE_LIB%" mkdir "%PACKAGE_LIB%"

xcopy "%CORE_SRC%" "%PACKAGE_LIB%\NativeSharp.Core\" /E /I /Y /Q >nul
if errorlevel 1 (
    echo [ERROR] Could not copy NativeSharp.Core.
    exit /b 1
)
xcopy "%COLL_SRC%" "%PACKAGE_LIB%\NativeSharp.Core.Collections.Generic\" /E /I /Y /Q >nul
if errorlevel 1 (
    echo [ERROR] Could not copy NativeSharp.Core.Collections.Generic.
    exit /b 1
)

>"%PACKAGE_LIB%\.source.txt" echo Synced from %NS_LIB_ROOT%
echo [LIB] Library synchronization complete.
endlocal & set "NS_LIB_READY=1"
exit /b 0
