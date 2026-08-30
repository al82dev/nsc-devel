@echo off
setlocal EnableExtensions
call "%~dp0..\..\tools\clean_project.cmd" "%~dp0"
exit /b %ERRORLEVEL%
