@echo off
setlocal
for %%O in (0 1 2 3) do (
  set "NSC_T08_OPT=%%O"
  call "%~dp0validar.cmd" || exit /b 1
  echo DLL preexistente y ruta aislada: O%%O correcto.
)
exit /b 0
