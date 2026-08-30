@echo off
setlocal EnableExtensions
set "PROJDIR=%~dp0"
call "%PROJDIR%..\..\tools\build_project.cmd" "%PROJDIR%Book08BinarySearchTree.nsproj" "%PROJDIR%Book08BinarySearchTree.exe"
exit /b %ERRORLEVEL%
