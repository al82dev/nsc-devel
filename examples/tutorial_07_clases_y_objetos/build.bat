@echo off
setlocal EnableExtensions
set "PROJDIR=%~dp0"
python "%PROJDIR%..\..\tools\build_project.py" "%PROJDIR%Tutorial07ClasesObjetos.nsproj" "%PROJDIR%Tutorial07ClasesObjetos.exe"
exit /b %ERRORLEVEL%
