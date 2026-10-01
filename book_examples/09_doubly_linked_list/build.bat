@echo off
setlocal EnableExtensions
set "PROJDIR=%~dp0"
python "%PROJDIR%..\..\tools\build_project.py" "%PROJDIR%Book09DoublyLinkedList.nsproj" "%PROJDIR%Book09DoublyLinkedList.exe"
exit /b %ERRORLEVEL%
