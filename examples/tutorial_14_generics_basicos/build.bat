@echo off
setlocal EnableExtensions
set "PROJDIR=%~dp0"
python "%PROJDIR%..\..\tools\build_project.py" "%PROJDIR%Tutorial14GenericsBasicos.nsproj" "%PROJDIR%Tutorial14GenericsBasicos.exe"
exit /b %ERRORLEVEL%
