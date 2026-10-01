@echo off
setlocal EnableExtensions
set "PROJDIR=%~dp0"
python "%PROJDIR%..\..\tools\build_project.py" "%PROJDIR%Book06StackExpressionEvaluator.nsproj" "%PROJDIR%Book06StackExpressionEvaluator.exe"
exit /b %ERRORLEVEL%
