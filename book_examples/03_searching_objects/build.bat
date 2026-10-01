@echo off
setlocal EnableExtensions
set "PROJDIR=%~dp0"
python "%PROJDIR%..\..\tools\build_project.py" "%PROJDIR%Book03SearchingObjects.nsproj" "%PROJDIR%Book03SearchingObjects.exe"
exit /b %ERRORLEVEL%
