@echo off
setlocal EnableExtensions
set "PROJDIR=%~dp0"
call "%PROJDIR%..\..\tools\build_project.cmd" "%PROJDIR%Tutorial07ClasesObjetos.nsproj" "%PROJDIR%Tutorial07ClasesObjetos.exe"
exit /b %ERRORLEVEL%
