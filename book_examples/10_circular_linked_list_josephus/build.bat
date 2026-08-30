@echo off
setlocal EnableExtensions
set "PROJDIR=%~dp0"
call "%PROJDIR%..\..\tools\build_project.cmd" "%PROJDIR%Book10CircularJosephus.nsproj" "%PROJDIR%Book10CircularJosephus.exe"
exit /b %ERRORLEVEL%
