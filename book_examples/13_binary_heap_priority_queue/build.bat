@echo off
setlocal EnableExtensions
set "PROJDIR=%~dp0"
python "%PROJDIR%..\..\tools\build_project.py" "%PROJDIR%Book13BinaryHeapPriorityQueue.nsproj" "%PROJDIR%Book13BinaryHeapPriorityQueue.exe"
exit /b %ERRORLEVEL%
