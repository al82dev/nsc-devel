@echo off
setlocal EnableDelayedExpansion
set "DEST=A:\nsc-interop-build\tanda08-flexible-var-volatile"
set "PATH=A:\al82dev-clang\bin;A:\nsc-interop-build\tanda1-ON\bin;%PATH%"
if not exist "%DEST%" mkdir "%DEST%"
if errorlevel 1 exit /b 2
for %%F in (Restricted.h VarVolatileMode.c VarVolatileMode.ns VarVolatileMS.nsproj VarVolatileISO.nsproj) do (
  copy /y "%~dp0%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 2
)
clang.exe -S -emit-llvm -O0 --target=x86_64-pc-windows-msvc -std=c11 -fms-volatile "%DEST%\VarVolatileMode.c" -o "%DEST%\clang-ms.ll"
if errorlevel 1 exit /b 2
clang.exe -S -emit-llvm -O0 --target=x86_64-pc-windows-msvc -std=c11 -fno-ms-volatile "%DEST%\VarVolatileMode.c" -o "%DEST%\clang-iso.ll"
if errorlevel 1 exit /b 2
for %%M in (MS ISO) do (
  nsc.exe --cpp-clang-root=A:\al82dev-clang --dump-project-oop-llvm "%DEST%\VarVolatile%%M.nsproj" > "%DEST%\nsc-%%M.ll"
  if errorlevel 1 exit /b 2
)
set "RESULT=0"
for %%M in (MS ISO) do (
  echo Comparacion %%M:
  python "%~dp0comparar-var-volatile.py" "%DEST%\clang-%%M.ll" "%DEST%\nsc-%%M.ll"
  if errorlevel 2 exit /b 2
  if errorlevel 1 set "RESULT=1"
)
exit /b %RESULT%
