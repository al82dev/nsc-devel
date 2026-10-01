@echo off
setlocal EnableExtensions
set "PROJDIR=%~dp0"
python "%PROJDIR%..\..\tools\build_project.py" "%PROJDIR%Tutorial10PropiedadesIndexadoresOperadores.nsproj" "%PROJDIR%Tutorial10PropiedadesIndexadoresOperadores.exe"
exit /b %ERRORLEVEL%
