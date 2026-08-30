@echo off
setlocal EnableExtensions
set "PROJDIR=%~dp0"
call "%PROJDIR%..\..\tools\build_project.cmd" "%PROJDIR%Tutorial02VariablesTipos.nsproj" "%PROJDIR%Tutorial02VariablesTipos.exe"
exit /b %ERRORLEVEL%
