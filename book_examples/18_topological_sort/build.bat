@echo off
setlocal EnableExtensions
set "PROJDIR=%~dp0"
call "%PROJDIR%..\..\tools\build_project.cmd" "%PROJDIR%Book18TopologicalSort.nsproj" "%PROJDIR%Book18TopologicalSort.exe"
exit /b %ERRORLEVEL%
