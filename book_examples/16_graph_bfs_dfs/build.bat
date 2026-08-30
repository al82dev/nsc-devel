@echo off
setlocal EnableExtensions
set "PROJDIR=%~dp0"
call "%PROJDIR%..\..\tools\build_project.cmd" "%PROJDIR%Book16GraphBfsDfs.nsproj" "%PROJDIR%Book16GraphBfsDfs.exe"
exit /b %ERRORLEVEL%
