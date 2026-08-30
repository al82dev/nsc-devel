@echo off
setlocal EnableExtensions
set "PROJDIR=%~dp0"
call "%PROJDIR%..\..\tools\build_project.cmd" "%PROJDIR%Book06StackExpressionEvaluator.nsproj" "%PROJDIR%Book06StackExpressionEvaluator.exe"
exit /b %ERRORLEVEL%
