@echo off
setlocal EnableExtensions
set "PROJDIR=%~dp0"
python "%PROJDIR%..\..\tools\build_project.py" "%PROJDIR%Tutorial01HelloMundo.nsproj" "%PROJDIR%Tutorial01HelloMundo.exe"
exit /b %ERRORLEVEL%
