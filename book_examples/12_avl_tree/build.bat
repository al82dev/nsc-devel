@echo off
setlocal EnableExtensions
set "PROJDIR=%~dp0"
call "%PROJDIR%..\..\tools\build_project.cmd" "%PROJDIR%Book12AvlTree.nsproj" "%PROJDIR%Book12AvlTree.exe"
exit /b %ERRORLEVEL%
