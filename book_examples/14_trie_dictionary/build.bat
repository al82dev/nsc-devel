@echo off
setlocal EnableExtensions
set "PROJDIR=%~dp0"
python "%PROJDIR%..\..\tools\build_project.py" "%PROJDIR%Book14TrieDictionary.nsproj" "%PROJDIR%Book14TrieDictionary.exe"
exit /b %ERRORLEVEL%
