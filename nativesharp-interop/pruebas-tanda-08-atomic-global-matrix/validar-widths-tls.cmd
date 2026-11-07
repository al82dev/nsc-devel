@echo off
call "%~dp0validar-widths.cmd" tls
exit /b %errorlevel%
