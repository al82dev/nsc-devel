@echo off
setlocal EnableExtensions
set "PROJDIR=%~dp0"
python "%PROJDIR%..\..\tools\build_project.py" "%PROJDIR%Tutorial18Eventos.nsproj" "%PROJDIR%Tutorial18Eventos.exe"
exit /b %ERRORLEVEL%
