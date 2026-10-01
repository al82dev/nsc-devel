@echo off
setlocal EnableExtensions
set "PROJDIR=%~dp0"
python "%PROJDIR%..\..\tools\build_project.py" "%PROJDIR%Tutorial12OwnershipModel.nsproj" "%PROJDIR%Tutorial12OwnershipModel.exe"
exit /b %ERRORLEVEL%
