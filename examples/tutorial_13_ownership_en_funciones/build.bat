@echo off
setlocal EnableExtensions
set "PROJDIR=%~dp0"
call "%PROJDIR%..\..\tools\build_project.cmd" "%PROJDIR%Tutorial13OwnershipEnFunciones.nsproj" "%PROJDIR%Tutorial13OwnershipEnFunciones.exe"
exit /b %ERRORLEVEL%
