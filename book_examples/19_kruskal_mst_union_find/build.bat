@echo off
setlocal EnableExtensions
set "PROJDIR=%~dp0"
call "%PROJDIR%..\..\tools\build_project.cmd" "%PROJDIR%Book19KruskalMst.nsproj" "%PROJDIR%Book19KruskalMst.exe"
exit /b %ERRORLEVEL%
