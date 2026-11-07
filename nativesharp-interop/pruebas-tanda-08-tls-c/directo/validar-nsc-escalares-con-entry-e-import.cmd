@echo off
setlocal
set "NSC_TLS_PROBE_IMPORT=-DTLS_PROBE_ENTRY -DTLS_PROBE_IMPORT"
set "NSC_TLS_PROBE_ENTRY=/entry:DllMain"
call "%~dp0validar-nsc-escalares.cmd"
exit /b %ERRORLEVEL%
