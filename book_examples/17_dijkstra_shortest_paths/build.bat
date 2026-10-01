@echo off
setlocal EnableExtensions
set "PROJDIR=%~dp0"
python "%PROJDIR%..\..\tools\build_project.py" "%PROJDIR%Book17DijkstraShortestPaths.nsproj" "%PROJDIR%Book17DijkstraShortestPaths.exe"
exit /b %ERRORLEVEL%
