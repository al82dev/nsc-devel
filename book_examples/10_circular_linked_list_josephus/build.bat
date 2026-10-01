@echo off
setlocal EnableExtensions
set "PROJDIR=%~dp0"
python "%PROJDIR%..\..\tools\build_project.py" "%PROJDIR%Book10CircularJosephus.nsproj" "%PROJDIR%Book10CircularJosephus.exe"
exit /b %ERRORLEVEL%
