@echo off
setlocal EnableExtensions
set "PROJDIR=%~dp0"
call "%PROJDIR%..\..\tools\build_project.cmd" "%PROJDIR%Book02PayrollOop.nsproj" "%PROJDIR%Book02PayrollOop.exe"
exit /b %ERRORLEVEL%
