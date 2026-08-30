@echo off
setlocal EnableExtensions
set "PROJDIR=%~dp0"
call "%PROJDIR%..\..\tools\build_project.cmd" "%PROJDIR%Tutorial04FuncionesRecursion.nsproj" "%PROJDIR%Tutorial04FuncionesRecursion.exe"
exit /b %ERRORLEVEL%
