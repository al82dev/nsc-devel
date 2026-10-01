@echo off
setlocal EnableExtensions
set "PROJDIR=%~dp0"
python "%PROJDIR%..\..\tools\build_project.py" "%PROJDIR%Book23PolynomialAlgebra.nsproj" "%PROJDIR%Book23PolynomialAlgebra.exe"
exit /b %ERRORLEVEL%
