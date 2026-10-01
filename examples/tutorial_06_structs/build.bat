@echo off
setlocal EnableExtensions
set "PROJDIR=%~dp0"
python "%PROJDIR%..\..\tools\build_project.py" "%PROJDIR%Tutorial06Structs.nsproj" "%PROJDIR%Tutorial06Structs.exe"
exit /b %ERRORLEVEL%
