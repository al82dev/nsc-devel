@echo off
setlocal
for %%O in (0 1 2 3) do (
  set "NSC_T08_OPT=%%O"
  for %%T in (alias funcion global inline global-static) do (
    echo === Dependencia C %%T de plantilla precompilada O%%O ===
    call "%~dp0..\pruebas-tanda-08-%%T-template\validar.cmd"
    if errorlevel 1 exit /b 1
  )
)
exit /b 0
