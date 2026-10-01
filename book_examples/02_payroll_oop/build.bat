@echo off
setlocal EnableExtensions
set "PROJDIR=%~dp0"
python "%PROJDIR%..\..\tools\build_project.py" "%PROJDIR%Book02PayrollOop.nsproj" "%PROJDIR%Book02PayrollOop.exe"
exit /b %ERRORLEVEL%
