@echo off
setlocal EnableExtensions
set "PROJDIR=%~dp0"
call "%PROJDIR%..\..\tools\build_project.cmd" "%PROJDIR%Tutorial03ControlDeFlujo.nsproj" "%PROJDIR%Tutorial03ControlDeFlujo.exe"
exit /b %ERRORLEVEL%
