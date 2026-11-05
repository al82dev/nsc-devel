@echo off
setlocal
set "DEST=A:\nsc-interop-build\cpp-fixtures\metadata-cpp-classification"
if not exist "%DEST%" mkdir "%DEST%"
call "%~dp0ejecutar-lib.cmd" > "%DEST%\private.log" 2>&1
if errorlevel 1 exit /b 1
findstr /l /c:"size requires a positive byte count" "%DEST%\private.log" >nul
if not errorlevel 1 exit /b 3
call "%~dp0ejecutar-api-publica.cmd" > "%DEST%\public.log" 2>&1
if not errorlevel 1 exit /b 4
findstr /l /c:"field 'value' refers to local type" "%DEST%\public.log" >nul
if errorlevel 1 exit /b 5
findstr /l /c:"Demo.Outer" "%DEST%\public.log" >nul
if errorlevel 1 exit /b 6
echo Tipo C++ privado no se publica como struct NS; tipo expuesto se rechaza explicitamente.
exit /b 0
