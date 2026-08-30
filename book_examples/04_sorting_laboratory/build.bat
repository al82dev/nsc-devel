@echo off
setlocal EnableExtensions
set "PROJDIR=%~dp0"
call "%PROJDIR%..\..\tools\build_project.cmd" "%PROJDIR%Book04SortingLaboratory.nsproj" "%PROJDIR%Book04SortingLaboratory.exe"
exit /b %ERRORLEVEL%
