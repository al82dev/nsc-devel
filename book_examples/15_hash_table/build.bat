@echo off
setlocal EnableExtensions
set "PROJDIR=%~dp0"
python "%PROJDIR%..\..\tools\build_project.py" "%PROJDIR%Book15HashTable.nsproj" "%PROJDIR%Book15HashTable.exe"
exit /b %ERRORLEVEL%
