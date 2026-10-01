@echo off
setlocal EnableExtensions
python "%~dp0..\..\tools\clean_project.py" "%~dp0"
exit /b %ERRORLEVEL%
