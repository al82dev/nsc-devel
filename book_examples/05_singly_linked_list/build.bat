@echo off
setlocal EnableExtensions
set "PROJDIR=%~dp0"
python "%PROJDIR%..\..\tools\build_project.py" "%PROJDIR%Book05SinglyLinkedList.nsproj" "%PROJDIR%Book05SinglyLinkedList.exe"
exit /b %ERRORLEVEL%
