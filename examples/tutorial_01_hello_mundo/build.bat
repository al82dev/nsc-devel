@echo off
setlocal EnableExtensions
set "PROJDIR=%~dp0"
call "%PROJDIR%..\..\tools\build_project.cmd" "%PROJDIR%Tutorial01HelloMundo.nsproj" "%PROJDIR%Tutorial01HelloMundo.exe"
exit /b %ERRORLEVEL%
