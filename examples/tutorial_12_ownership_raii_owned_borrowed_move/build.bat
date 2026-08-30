@echo off
setlocal EnableExtensions
set "PROJDIR=%~dp0"
call "%PROJDIR%..\..\tools\build_project.cmd" "%PROJDIR%Tutorial12OwnershipModel.nsproj" "%PROJDIR%Tutorial12OwnershipModel.exe"
exit /b %ERRORLEVEL%
