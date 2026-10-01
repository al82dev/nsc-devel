@echo off
setlocal EnableExtensions
set "PROJDIR=%~dp0"
python "%PROJDIR%..\..\tools\build_project.py" "%PROJDIR%Book04SortingLaboratory.nsproj" "%PROJDIR%Book04SortingLaboratory.exe"
exit /b %ERRORLEVEL%
