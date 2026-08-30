@echo off
setlocal EnableExtensions
set "PROJDIR=%~dp0"
call "%PROJDIR%..\..\tools\build_project.cmd" "%PROJDIR%Tutorial14GenericsBasicos.nsproj" "%PROJDIR%Tutorial14GenericsBasicos.exe"
exit /b %ERRORLEVEL%
