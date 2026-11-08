@echo off
setlocal
for %%O in (0 1 2 3) do (
  set "NSC_T08_OPT=%%O"
  call "%~dp0externa-dos-bibliotecas\validar.cmd" tls || exit /b 1
  call "%~dp0externa-dos-bibliotecas\validar.cmd" tls-hilos || exit /b 1
  call "%~dp0static-dos-bibliotecas-mutables\validar-tls.cmd" || exit /b 1
  call "%~dp0static-dos-bibliotecas-mutables\validar-tls.cmd" hilos || exit /b 1
  echo TLS externa e interna con y sin hilos: O%%O correcto.
)
exit /b 0
