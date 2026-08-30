@echo off
setlocal EnableExtensions
set "PROJDIR=%~dp0"
call "%PROJDIR%..\..\tools\build_project.cmd" "%PROJDIR%Book23PolynomialAlgebra.nsproj" "%PROJDIR%Book23PolynomialAlgebra.exe"
exit /b %ERRORLEVEL%
