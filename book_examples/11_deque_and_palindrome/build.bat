@echo off
setlocal EnableExtensions
set "PROJDIR=%~dp0"
call "%PROJDIR%..\..\tools\build_project.cmd" "%PROJDIR%Book11DequeAndPalindrome.nsproj" "%PROJDIR%Book11DequeAndPalindrome.exe"
exit /b %ERRORLEVEL%
