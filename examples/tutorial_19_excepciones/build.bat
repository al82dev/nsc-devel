@echo off
setlocal EnableExtensions
set "PROJDIR=%~dp0"
call "%PROJDIR%..\..\tools\build_project.cmd" "%PROJDIR%Tutorial19Excepciones.nsproj" "%PROJDIR%Tutorial19Excepciones.exe"
exit /b %ERRORLEVEL%
