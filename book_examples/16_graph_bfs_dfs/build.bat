@echo off
setlocal EnableExtensions
set "PROJDIR=%~dp0"
python "%PROJDIR%..\..\tools\build_project.py" "%PROJDIR%Book16GraphBfsDfs.nsproj" "%PROJDIR%Book16GraphBfsDfs.exe"
exit /b %ERRORLEVEL%
