@echo off
setlocal EnableExtensions
set "PROJDIR=%~dp0"
call "%PROJDIR%..\..\tools\build_project.cmd" "%PROJDIR%Book17DijkstraShortestPaths.nsproj" "%PROJDIR%Book17DijkstraShortestPaths.exe"
exit /b %ERRORLEVEL%
