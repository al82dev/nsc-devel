@echo off
setlocal EnableExtensions
set "PROJDIR=%~dp0"
call "%PROJDIR%..\..\tools\build_project.cmd" "%PROJDIR%Tutorial10PropiedadesIndexadoresOperadores.nsproj" "%PROJDIR%Tutorial10PropiedadesIndexadoresOperadores.exe"
exit /b %ERRORLEVEL%
