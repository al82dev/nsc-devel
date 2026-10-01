@echo off
setlocal EnableExtensions
set "PROJDIR=%~dp0"
python "%PROJDIR%..\..\tools\build_project.py" "%PROJDIR%Tutorial13OwnershipEnFunciones.nsproj" "%PROJDIR%Tutorial13OwnershipEnFunciones.exe"
exit /b %ERRORLEVEL%
