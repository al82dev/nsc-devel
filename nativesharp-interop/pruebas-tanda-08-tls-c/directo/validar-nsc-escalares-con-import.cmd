@echo off
setlocal
set "NSC_TLS_PROBE_IMPORT=-DTLS_PROBE_IMPORT"
call "%~dp0validar-nsc-escalares.cmd"
exit /b %ERRORLEVEL%
