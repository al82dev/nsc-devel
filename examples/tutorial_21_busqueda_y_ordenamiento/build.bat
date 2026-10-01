@echo off
setlocal EnableExtensions
set "PROJDIR=%~dp0"
python "%PROJDIR%..\..\tools\build_project.py" "%PROJDIR%Tutorial21BusquedaYOrdenamiento.nsproj" "%PROJDIR%Tutorial21BusquedaYOrdenamiento.exe"
exit /b %ERRORLEVEL%
