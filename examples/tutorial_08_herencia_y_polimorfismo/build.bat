@echo off
setlocal EnableExtensions
set "PROJDIR=%~dp0"
call "%PROJDIR%..\..\tools\build_project.cmd" "%PROJDIR%Tutorial08HerenciaPolimorfismo.nsproj" "%PROJDIR%Tutorial08HerenciaPolimorfismo.exe"
exit /b %ERRORLEVEL%
