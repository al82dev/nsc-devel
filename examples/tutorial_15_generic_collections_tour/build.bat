@echo off
setlocal EnableExtensions
set "PROJDIR=%~dp0"
call "%PROJDIR%..\..\tools\build_project.cmd" "%PROJDIR%Tutorial15GenericCollectionsTour.nsproj" "%PROJDIR%Tutorial15GenericCollectionsTour.exe"
exit /b %ERRORLEVEL%
