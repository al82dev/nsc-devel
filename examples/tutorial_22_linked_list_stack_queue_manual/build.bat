@echo off
setlocal EnableExtensions
set "PROJDIR=%~dp0"
call "%PROJDIR%..\..\tools\build_project.cmd" "%PROJDIR%Tutorial22LinkedListStackQueueManual.nsproj" "%PROJDIR%Tutorial22LinkedListStackQueueManual.exe"
exit /b %ERRORLEVEL%
