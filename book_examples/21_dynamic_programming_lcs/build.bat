@echo off
setlocal EnableExtensions
set "PROJDIR=%~dp0"
python "%PROJDIR%..\..\tools\build_project.py" "%PROJDIR%Book21LongestCommonSubsequence.nsproj" "%PROJDIR%Book21LongestCommonSubsequence.exe"
exit /b %ERRORLEVEL%
