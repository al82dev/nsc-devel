@echo off
setlocal EnableExtensions
set "PROJDIR=%~dp0"
call "%PROJDIR%..\..\tools\build_project.cmd" "%PROJDIR%Tutorial20RecursionClasica.nsproj" "%PROJDIR%Tutorial20RecursionClasica.exe"
exit /b %ERRORLEVEL%
