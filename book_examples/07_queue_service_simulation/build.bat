@echo off
setlocal EnableExtensions
set "PROJDIR=%~dp0"
call "%PROJDIR%..\..\tools\build_project.cmd" "%PROJDIR%Book07QueueServiceSimulation.nsproj" "%PROJDIR%Book07QueueServiceSimulation.exe"
exit /b %ERRORLEVEL%
