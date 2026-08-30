@echo off
setlocal EnableExtensions
set "PROJDIR=%~dp0"
call "%PROJDIR%..\..\tools\build_project.cmd" "%PROJDIR%Tutorial23HuffmanPriorityQueue.nsproj" "%PROJDIR%Tutorial23HuffmanPriorityQueue.exe"
exit /b %ERRORLEVEL%
