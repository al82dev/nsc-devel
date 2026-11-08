@echo off
setlocal
for %%O in (0 1 2 3) do (
  python "%~dp0validar.py" %%O --headers-hidden
  if errorlevel 1 exit /b 1
  python "%~dp0validar-invocacion.py" %%O
  if errorlevel 1 exit /b 1
  python "%~dp0validar-invocacion.py" %%O --nested
  if errorlevel 1 exit /b 1
  python "%~dp0validar-retorno.py" %%O
  if errorlevel 1 exit /b 1
  python "%~dp0validar-retorno.py" %%O --large
  if errorlevel 1 exit /b 1
  python "%~dp0validar-enum.py" %%O
  if errorlevel 1 exit /b 1
  python "%~dp0validar-enum.py" %%O --returns
  if errorlevel 1 exit /b 1
)
python "%~dp0validar.py" 0 --incompatible --headers-hidden
if errorlevel 1 exit /b 1
python "%~dp0validar.py" 0 --same-size-incompatible --headers-hidden
if errorlevel 1 exit /b 1
python "%~dp0validar.py" 0 --nested --same-size-incompatible --headers-hidden
if errorlevel 1 exit /b 1
python "%~dp0validar.py" 0 --header-changed
if errorlevel 1 exit /b 1
python "%~dp0validar-enum.py" 0 --mismatch
if errorlevel 1 exit /b 1
python "%~dp0validar-enum.py" 0 --returns --mismatch
if errorlevel 1 exit /b 1
python "%~dp0validar-enum.py" 0 --consumer-ambiguous
if errorlevel 1 exit /b 1
exit /b 0
