@echo off
setlocal EnableExtensions
set "PROJDIR=%~dp0"
python "%PROJDIR%..\..\tools\build_project.py" "%PROJDIR%Tutorial05ArraysStrings.nsproj" "%PROJDIR%Tutorial05ArraysStrings.exe"
exit /b %ERRORLEVEL%
