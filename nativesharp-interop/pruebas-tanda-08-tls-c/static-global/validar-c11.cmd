@echo off
setlocal
python "%~dp0preparar_c11.py"
if errorlevel 1 exit /b 1
call A:\nsc-interop-build\tls-abi\c11-fixture\validar.cmd
exit /b %ERRORLEVEL%
