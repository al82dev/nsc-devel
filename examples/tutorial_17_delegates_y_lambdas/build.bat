@echo off
setlocal EnableExtensions
set "PROJDIR=%~dp0"
call "%PROJDIR%..\..\tools\build_project.cmd" "%PROJDIR%Tutorial17DelegatesLambdas.nsproj" "%PROJDIR%Tutorial17DelegatesLambdas.exe"
exit /b %ERRORLEVEL%
