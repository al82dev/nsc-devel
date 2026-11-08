@echo off
setlocal
set "NSC_T08_CACHE=1"
for %%O in (0 1 2 3) do (
  set "NSC_T08_OPT=%%O"
  call "%~dp0validar-mismo-nombre.cmd"
  if errorlevel 1 exit /b 1
  python "%~dp0validar-variadicas.py" %%O
  if errorlevel 1 exit /b 1
  python "%~dp0validar-variadicas-puntero.py" %%O
  if errorlevel 1 exit /b 1
  python "%~dp0validar-variadicas-puntero.py" %%O --same-alias
  if errorlevel 1 exit /b 1
  if "%%O"=="0" python "%~dp0validar-variadicas-puntero.py" %%O --mismatch
  if errorlevel 1 exit /b 1
  echo DLL homonimas con cache: O%%O correcto.
)
exit /b 0
