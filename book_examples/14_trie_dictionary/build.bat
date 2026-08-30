@echo off
setlocal EnableExtensions
set "PROJDIR=%~dp0"
call "%PROJDIR%..\..\tools\build_project.cmd" "%PROJDIR%Book14TrieDictionary.nsproj" "%PROJDIR%Book14TrieDictionary.exe"
exit /b %ERRORLEVEL%
