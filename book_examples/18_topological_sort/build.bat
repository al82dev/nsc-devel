@echo off
setlocal EnableExtensions
set "PROJDIR=%~dp0"
python "%PROJDIR%..\..\tools\build_project.py" "%PROJDIR%Book18TopologicalSort.nsproj" "%PROJDIR%Book18TopologicalSort.exe"
exit /b %ERRORLEVEL%
