@echo off
setlocal EnableExtensions
set "PROJDIR=%~dp0"
python "%PROJDIR%..\..\tools\build_project.py" "%PROJDIR%Tutorial19Excepciones.nsproj" "%PROJDIR%Tutorial19Excepciones.exe"
exit /b %ERRORLEVEL%
