@echo off
call "%~dp0validar.cmd" tls
exit /b %errorlevel%
