@echo off
setlocal EnableExtensions
set "PROJDIR=%~dp0"
call "%PROJDIR%..\..\tools\build_project.cmd" "%PROJDIR%Book24VectorMatrixAlgebra.nsproj" "%PROJDIR%Book24VectorMatrixAlgebra.exe"
exit /b %ERRORLEVEL%
