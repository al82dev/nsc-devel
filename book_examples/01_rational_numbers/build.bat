@echo off
setlocal EnableExtensions
set "PROJDIR=%~dp0"
python "%PROJDIR%..\..\tools\build_project.py" "%PROJDIR%Book01RationalNumbers.nsproj" "%PROJDIR%Book01RationalNumbers.exe"
exit /b %ERRORLEVEL%
