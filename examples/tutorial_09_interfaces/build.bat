@echo off
setlocal EnableExtensions
set "PROJDIR=%~dp0"
python "%PROJDIR%..\..\tools\build_project.py" "%PROJDIR%Tutorial09Interfaces.nsproj" "%PROJDIR%Tutorial09Interfaces.exe"
exit /b %ERRORLEVEL%
