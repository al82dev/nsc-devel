@echo off
setlocal EnableExtensions
set "PROJDIR=%~dp0"
python "%PROJDIR%..\..\tools\build_project.py" "%PROJDIR%Tutorial20RecursionClasica.nsproj" "%PROJDIR%Tutorial20RecursionClasica.exe"
exit /b %ERRORLEVEL%
