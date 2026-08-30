@echo off
setlocal EnableExtensions
set "PROJDIR=%~dp0"
call "%PROJDIR%..\..\tools\build_project.cmd" "%PROJDIR%Tutorial09Interfaces.nsproj" "%PROJDIR%Tutorial09Interfaces.exe"
exit /b %ERRORLEVEL%
