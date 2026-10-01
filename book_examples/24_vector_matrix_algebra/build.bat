@echo off
setlocal EnableExtensions
set "PROJDIR=%~dp0"
python "%PROJDIR%..\..\tools\build_project.py" "%PROJDIR%Book24VectorMatrixAlgebra.nsproj" "%PROJDIR%Book24VectorMatrixAlgebra.exe"
exit /b %ERRORLEVEL%
