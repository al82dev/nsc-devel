@echo off
setlocal
set "PATH=A:\al82dev-clang\bin;%PATH%"
python "%~dp0diagnosticar-import-directo.py"
exit /b %errorlevel%
