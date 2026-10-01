@echo off
setlocal EnableExtensions
set "PROJDIR=%~dp0"
python "%PROJDIR%..\..\tools\build_project.py" "%PROJDIR%Tutorial08HerenciaPolimorfismo.nsproj" "%PROJDIR%Tutorial08HerenciaPolimorfismo.exe"
exit /b %ERRORLEVEL%
