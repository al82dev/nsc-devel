@echo off
setlocal EnableExtensions
set "PROJDIR=%~dp0"
call "%PROJDIR%..\..\tools\build_project.cmd" "%PROJDIR%Book20KnapsackDynamicProgramming.nsproj" "%PROJDIR%Book20KnapsackDynamicProgramming.exe"
exit /b %ERRORLEVEL%
