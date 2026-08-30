@echo off
setlocal EnableExtensions
set "PROJDIR=%~dp0"
call "%PROJDIR%..\..\tools\build_project.cmd" "%PROJDIR%Book05SinglyLinkedList.nsproj" "%PROJDIR%Book05SinglyLinkedList.exe"
exit /b %ERRORLEVEL%
