@echo off
setlocal EnableExtensions
set "PROJDIR=%~dp0"
call "%PROJDIR%..\..\tools\build_project.cmd" "%PROJDIR%Book15HashTable.nsproj" "%PROJDIR%Book15HashTable.exe"
exit /b %ERRORLEVEL%
