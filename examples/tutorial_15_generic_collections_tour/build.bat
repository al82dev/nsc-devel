@echo off
setlocal EnableExtensions
set "PROJDIR=%~dp0"
python "%PROJDIR%..\..\tools\build_project.py" "%PROJDIR%Tutorial15GenericCollectionsTour.nsproj" "%PROJDIR%Tutorial15GenericCollectionsTour.exe"
exit /b %ERRORLEVEL%
