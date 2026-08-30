@echo off
setlocal EnableExtensions
set "PROJDIR=%~dp0"
call "%PROJDIR%..\..\tools\build_project.cmd" "%PROJDIR%Book09DoublyLinkedList.nsproj" "%PROJDIR%Book09DoublyLinkedList.exe"
exit /b %ERRORLEVEL%
