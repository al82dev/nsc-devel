@echo off
setlocal EnableExtensions
set "PROJDIR=%~dp0"
python "%PROJDIR%..\..\tools\build_project.py" "%PROJDIR%Tutorial17DelegatesLambdas.nsproj" "%PROJDIR%Tutorial17DelegatesLambdas.exe"
exit /b %ERRORLEVEL%
