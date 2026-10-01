@echo off
setlocal EnableExtensions
set "PROJDIR=%~dp0"
python "%PROJDIR%..\..\tools\build_project.py" "%PROJDIR%Book08BinarySearchTree.nsproj" "%PROJDIR%Book08BinarySearchTree.exe"
exit /b %ERRORLEVEL%
