@echo off
setlocal EnableExtensions
set "PROJDIR=%~dp0"
call "%PROJDIR%..\..\tools\build_project.cmd" "%PROJDIR%Book13BinaryHeapPriorityQueue.nsproj" "%PROJDIR%Book13BinaryHeapPriorityQueue.exe"
exit /b %ERRORLEVEL%
