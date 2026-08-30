@echo off
setlocal EnableExtensions
set "PROJDIR=%~dp0"
call "%PROJDIR%..\..\tools\build_project.cmd" "%PROJDIR%Tutorial11RttiIsAs.nsproj" "%PROJDIR%Tutorial11RttiIsAs.exe"
exit /b %ERRORLEVEL%
