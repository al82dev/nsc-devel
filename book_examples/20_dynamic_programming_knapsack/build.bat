@echo off
setlocal EnableExtensions
set "PROJDIR=%~dp0"
python "%PROJDIR%..\..\tools\build_project.py" "%PROJDIR%Book20KnapsackDynamicProgramming.nsproj" "%PROJDIR%Book20KnapsackDynamicProgramming.exe"
exit /b %ERRORLEVEL%
