@echo off
setlocal EnableExtensions
set "PROJDIR=%~dp0"
call "%PROJDIR%..\..\tools\build_project.cmd" "%PROJDIR%Tutorial18Eventos.nsproj" "%PROJDIR%Tutorial18Eventos.exe"
exit /b %ERRORLEVEL%
