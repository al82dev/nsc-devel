@echo off
setlocal EnableExtensions
set "PROJDIR=%~dp0"
python "%PROJDIR%..\..\tools\build_project.py" "%PROJDIR%Tutorial23HuffmanPriorityQueue.nsproj" "%PROJDIR%Tutorial23HuffmanPriorityQueue.exe"
exit /b %ERRORLEVEL%
