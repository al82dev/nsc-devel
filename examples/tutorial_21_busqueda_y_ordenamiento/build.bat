@echo off
setlocal EnableExtensions
set "PROJDIR=%~dp0"
call "%PROJDIR%..\..\tools\build_project.cmd" "%PROJDIR%Tutorial21BusquedaYOrdenamiento.nsproj" "%PROJDIR%Tutorial21BusquedaYOrdenamiento.exe"
exit /b %ERRORLEVEL%
