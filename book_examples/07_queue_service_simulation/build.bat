@echo off
setlocal EnableExtensions
set "PROJDIR=%~dp0"
python "%PROJDIR%..\..\tools\build_project.py" "%PROJDIR%Book07QueueServiceSimulation.nsproj" "%PROJDIR%Book07QueueServiceSimulation.exe"
exit /b %ERRORLEVEL%
