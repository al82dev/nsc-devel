@echo off
setlocal EnableExtensions
set "PROJDIR=%~dp0"
call "%PROJDIR%..\..\tools\build_project.cmd" "%PROJDIR%Book21LongestCommonSubsequence.nsproj" "%PROJDIR%Book21LongestCommonSubsequence.exe"
exit /b %ERRORLEVEL%
