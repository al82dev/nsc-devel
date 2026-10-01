@echo off
setlocal EnableExtensions
set "PROJDIR=%~dp0"
python "%PROJDIR%..\..\tools\build_project.py" "%PROJDIR%Tutorial02VariablesTipos.nsproj" "%PROJDIR%Tutorial02VariablesTipos.exe"
exit /b %ERRORLEVEL%
