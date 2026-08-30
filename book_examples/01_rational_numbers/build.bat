@echo off
setlocal EnableExtensions
set "PROJDIR=%~dp0"
call "%PROJDIR%..\..\tools\build_project.cmd" "%PROJDIR%Book01RationalNumbers.nsproj" "%PROJDIR%Book01RationalNumbers.exe"
exit /b %ERRORLEVEL%
