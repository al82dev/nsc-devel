@echo off
setlocal EnableExtensions
set "PROJDIR=%~dp0"
call "%PROJDIR%..\..\tools\build_project.cmd" "%PROJDIR%Book22EditDistance.nsproj" "%PROJDIR%Book22EditDistance.exe"
exit /b %ERRORLEVEL%
