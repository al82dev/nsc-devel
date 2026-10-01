@echo off
setlocal EnableExtensions

set "PACKAGE_LIB=%~dp0..\lib"
set "CORE_SRC=%PACKAGE_LIB%\NativeSharp.Core"
set "COLL_SRC=%PACKAGE_LIB%\NativeSharp.Core.Collections.Generic"

if not exist "%CORE_SRC%\NativeSharp.Core.nsproj" (
    echo [ERROR] Missing bundled NativeSharp.Core project:
    echo         %CORE_SRC%\NativeSharp.Core.nsproj
    exit /b 1
)
if not exist "%COLL_SRC%\NativeSharp.Core.Collections.Generic.nsproj" (
    echo [ERROR] Missing bundled NativeSharp.Core.Collections.Generic project:
    echo         %COLL_SRC%\NativeSharp.Core.Collections.Generic.nsproj
    exit /b 1
)

echo [LIB] Using bundled project libraries under "%PACKAGE_LIB%".
endlocal & set "NS_LIB_READY=1"
exit /b 0
