@echo off
setlocal
set "DEST=A:\nsc-interop-build\tls-abi\empty-import-nsc"
if not exist "%DEST%" mkdir "%DEST%"
copy /y A:\nsc-interop-build\tanda08-tls-directo\scalars\scalar-O0.exe "%DEST%\scalar.exe" >nul
if errorlevel 1 exit /b 1
copy /y A:\nsc-interop-build\tanda08-tls-directo\scalars\api.dll "%DEST%\api.dll" >nul
if errorlevel 1 exit /b 1
"%DEST%\scalar.exe"
set "ORIGINAL_STATUS=%ERRORLEVEL%"
echo Original status: %ORIGINAL_STATUS%.
if not "%ORIGINAL_STATUS%"=="8" exit /b 1
copy /y A:\nsc-interop-build\tls-abi\tls_import_empty_inside_nosentinel.dll "%DEST%\api.dll" >nul
if errorlevel 1 exit /b 1
"%DEST%\scalar.exe"
set "MODIFIED_STATUS=%ERRORLEVEL%"
echo Experimental copy status: %MODIFIED_STATUS%.
if not "%MODIFIED_STATUS%"=="0" exit /b 1
exit /b 0
