@echo off
setlocal EnableExtensions
set "PROJDIR=%~dp0"
python "%PROJDIR%..\..\tools\build_project.py" "%PROJDIR%Book12AvlTree.nsproj" "%PROJDIR%Book12AvlTree.exe"
exit /b %ERRORLEVEL%
