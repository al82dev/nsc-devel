@echo off
setlocal EnableDelayedExpansion
set "DEST=A:\nsc-interop-build\tanda08-flexible-var-volatile"
set "PATH=A:\al82dev-clang\bin;A:\nsc-interop-build\tanda1-ON\bin;%PATH%"
if not exist "%DEST%" mkdir "%DEST%"
if errorlevel 1 exit /b 2
for %%F in (Restricted.h VarVolatileMode.c VarVolatileMode.ns VarVolatileMS.nsproj VarVolatileISO.nsproj VarVolatileMixed.nsproj) do (
  copy /y "%~dp0%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 2
)
set "RESULT=0"
for %%O in (0 1 2 3) do (
  clang.exe -S -emit-llvm -O%%O --target=x86_64-pc-windows-msvc -std=c11 -fms-volatile "%DEST%\VarVolatileMode.c" -o "%DEST%\clang-ms-O%%O.ll"
  if errorlevel 1 exit /b 2
  clang.exe -S -emit-llvm -O%%O --target=x86_64-pc-windows-msvc -std=c11 -fno-ms-volatile "%DEST%\VarVolatileMode.c" -o "%DEST%\clang-iso-O%%O.ll"
  if errorlevel 1 exit /b 2
  for %%M in (MS ISO) do (
    if %%O==0 (
      nsc.exe --cpp-clang-root=A:\al82dev-clang --dump-project-oop-llvm "%DEST%\VarVolatile%%M.nsproj" > "%DEST%\nsc-%%M-O%%O.ll"
    ) else (
      nsc.exe --cpp-clang-root=A:\al82dev-clang --dump-project-oop-llvm-opt "%DEST%\VarVolatile%%M.nsproj" -O%%O > "%DEST%\nsc-%%M-O%%O.ll"
    )
    if errorlevel 1 exit /b 2
    echo Comparacion %%M O%%O:
    python "%~dp0comparar-var-volatile.py" "%DEST%\clang-%%M-O%%O.ll" "%DEST%\nsc-%%M-O%%O.ll"
    if errorlevel 2 exit /b 2
    if errorlevel 1 set "RESULT=1"
  )
  if %%O==0 (
    nsc.exe --cpp-clang-root=A:\al82dev-clang --dump-project-oop-llvm "%DEST%\VarVolatileMixed.nsproj" > "%DEST%\nsc-mixed-O%%O.ll"
  ) else (
    nsc.exe --cpp-clang-root=A:\al82dev-clang --dump-project-oop-llvm-opt "%DEST%\VarVolatileMixed.nsproj" -O%%O > "%DEST%\nsc-mixed-O%%O.ll"
  )
  if errorlevel 1 exit /b 2
  for %%M in (MS ISO) do (
    echo Comparacion referencias mixtas frente a %%M O%%O:
    python "%~dp0comparar-var-volatile.py" "%DEST%\clang-%%M-O%%O.ll" "%DEST%\nsc-mixed-O%%O.ll"
    if errorlevel 2 exit /b 2
    if errorlevel 1 set "RESULT=1"
  )
)
exit /b %RESULT%
