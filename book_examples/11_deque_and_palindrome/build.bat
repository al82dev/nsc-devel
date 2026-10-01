@echo off
setlocal EnableExtensions
set "PROJDIR=%~dp0"
python "%PROJDIR%..\..\tools\build_project.py" "%PROJDIR%Book11DequeAndPalindrome.nsproj" "%PROJDIR%Book11DequeAndPalindrome.exe"
exit /b %ERRORLEVEL%
