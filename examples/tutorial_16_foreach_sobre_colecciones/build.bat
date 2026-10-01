@echo off
setlocal EnableExtensions
set "PROJDIR=%~dp0"
python "%PROJDIR%..\..\tools\build_project.py" "%PROJDIR%Tutorial16ForeachSobreColecciones.nsproj" "%PROJDIR%Tutorial16ForeachSobreColecciones.exe"
exit /b %ERRORLEVEL%
