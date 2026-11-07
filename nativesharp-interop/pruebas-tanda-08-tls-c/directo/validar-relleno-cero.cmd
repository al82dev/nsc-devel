@echo off
setlocal
set "DEST=A:\nsc-interop-build\tls-abi\zerofill"
call "%~dp0validar-escalar-crt.cmd"
if errorlevel 1 exit /b 1
python "%~dp0prepare_zero_fill.py"
if errorlevel 1 exit /b 1
"%DEST%\crt-control.exe"
if errorlevel 1 (echo Control C relleno TLS fallo. & exit /b 1)
echo Control C relleno TLS correcto.
for %%O in (0 1 2 3) do (
  "%DEST%\crt-scalar-O%%O.exe"
  if errorlevel 1 (echo NS TLS relleno O%%O fallo. & exit /b 1)
  echo NS TLS relleno O%%O correcto.
)
exit /b 0
