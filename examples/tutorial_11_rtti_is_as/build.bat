@echo off
setlocal EnableExtensions
set "PROJDIR=%~dp0"
python "%PROJDIR%..\..\tools\build_project.py" "%PROJDIR%Tutorial11RttiIsAs.nsproj" "%PROJDIR%Tutorial11RttiIsAs.exe"
exit /b %ERRORLEVEL%
