@echo off
setlocal EnableExtensions
set "PROJDIR=%~dp0"
python "%PROJDIR%..\..\tools\build_project.py" "%PROJDIR%Tutorial03ControlDeFlujo.nsproj" "%PROJDIR%Tutorial03ControlDeFlujo.exe"
exit /b %ERRORLEVEL%
