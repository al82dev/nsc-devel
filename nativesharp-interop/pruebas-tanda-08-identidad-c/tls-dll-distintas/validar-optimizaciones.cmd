@echo off
setlocal
for %%O in (0 1 2 3) do (
  set "NSC_T08_OPT=%%O"
  call "%~dp0validar.cmd" dos || exit /b 1
  echo TLS de dos DLL distintas: O%%O correcto.
)
exit /b 0
