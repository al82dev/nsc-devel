@echo off
setlocal EnableExtensions

if not defined NS_ROOT set "NS_ROOT=C:\native-sharp"
if not defined NSC_EXE set "NSC_EXE=%NS_ROOT%\bin\nsc.exe"
if not defined NS_LIB_ROOT set "NS_LIB_ROOT=%NS_ROOT%\lib"

if not exist "%NSC_EXE%" (
    echo [ERROR] NativeSharp compiler not found:
    echo         %NSC_EXE%
    echo.
    echo Expected default layout:
    echo   C:\native-sharp\bin\nsc.exe
    echo   C:\native-sharp\lib\...
    echo.
    echo You may override it before building:
    echo   set NS_ROOT=D:\path\to\native-sharp
    echo or
    echo   set NSC_EXE=D:\path\to\nsc.exe
    exit /b 1
)

if not exist "%NS_LIB_ROOT%" (
    echo [ERROR] NativeSharp library directory not found:
    echo         %NS_LIB_ROOT%
    exit /b 1
)

endlocal & set "NS_ROOT=%NS_ROOT%" & set "NSC_EXE=%NSC_EXE%" & set "NS_LIB_ROOT=%NS_LIB_ROOT%"
exit /b 0
