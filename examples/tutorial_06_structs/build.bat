@echo off
setlocal EnableExtensions
set "PROJDIR=%~dp0"
call "%PROJDIR%..\..\tools\build_project.cmd" "%PROJDIR%Tutorial06Structs.nsproj" "%PROJDIR%Tutorial06Structs.exe"
exit /b %ERRORLEVEL%
