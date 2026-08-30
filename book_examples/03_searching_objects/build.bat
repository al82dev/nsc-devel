@echo off
setlocal EnableExtensions
set "PROJDIR=%~dp0"
call "%PROJDIR%..\..\tools\build_project.cmd" "%PROJDIR%Book03SearchingObjects.nsproj" "%PROJDIR%Book03SearchingObjects.exe"
exit /b %ERRORLEVEL%
