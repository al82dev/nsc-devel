@echo off
setlocal EnableExtensions
set "PROJDIR=%~dp0"
python "%PROJDIR%..\..\tools\build_project.py" "%PROJDIR%Book19KruskalMst.nsproj" "%PROJDIR%Book19KruskalMst.exe"
exit /b %ERRORLEVEL%
