@echo off
setlocal EnableExtensions
set "PROJDIR=%~dp0"
call "%PROJDIR%..\..\tools\build_project.cmd" "%PROJDIR%Tutorial16ForeachSobreColecciones.nsproj" "%PROJDIR%Tutorial16ForeachSobreColecciones.exe"
exit /b %ERRORLEVEL%
