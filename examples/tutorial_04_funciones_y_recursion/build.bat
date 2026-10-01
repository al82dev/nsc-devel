@echo off
setlocal EnableExtensions
set "PROJDIR=%~dp0"
python "%PROJDIR%..\..\tools\build_project.py" "%PROJDIR%Tutorial04FuncionesRecursion.nsproj" "%PROJDIR%Tutorial04FuncionesRecursion.exe"
exit /b %ERRORLEVEL%
