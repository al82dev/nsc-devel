@echo off
setlocal EnableExtensions
set "PROJDIR=%~dp0"
python "%PROJDIR%..\..\tools\build_project.py" "%PROJDIR%Book22EditDistance.nsproj" "%PROJDIR%Book22EditDistance.exe"
exit /b %ERRORLEVEL%
