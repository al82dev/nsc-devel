@echo off
setlocal EnableExtensions
set "PROJDIR=%~dp0"
python "%PROJDIR%..\..\tools\build_project.py" "%PROJDIR%Tutorial22LinkedListStackQueueManual.nsproj" "%PROJDIR%Tutorial22LinkedListStackQueueManual.exe"
exit /b %ERRORLEVEL%
