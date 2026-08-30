@echo off
setlocal EnableExtensions
set "PROJDIR=%~dp0"
call "%PROJDIR%..\..\tools\build_project.cmd" "%PROJDIR%Tutorial05ArraysStrings.nsproj" "%PROJDIR%Tutorial05ArraysStrings.exe"
exit /b %ERRORLEVEL%
