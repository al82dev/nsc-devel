@echo off
setlocal
set "DEST=A:\nsc-interop-build\tanda08-tls-c"
set "PATH=A:\al82dev-clang\bin;A:\nsc-interop-build\tanda1-ON\bin;%PATH%"
if not exist "%DEST%" mkdir "%DEST%"
if errorlevel 1 exit /b 1
set "TMP=%DEST%"
set "TEMP=%DEST%"
set "NSC_LINKER=A:\al82dev-clang\bin\lld-link.exe"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
copy /y "%~dp0Project.nsproj" "%DEST%\Project.nsproj" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0Main.ns" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0Api.h" "%DEST%\Api.h" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0Api.c" "%DEST%\Api.c" >nul
if errorlevel 1 exit /b 1
type nul > "%DEST%\Crt.h"
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 "%DEST%\Api.c" -o "%DEST%\api.obj" > "%DEST%\compile.log" 2>&1
if errorlevel 1 (echo Compilacion C TLS fallo; revisar compile.log. & exit /b 1)
llvm-lib.exe /nologo /out:"%DEST%\api.lib" "%DEST%\api.obj" > "%DEST%\archive.log" 2>&1
if errorlevel 1 (echo Creacion de biblioteca TLS fallo; revisar archive.log. & exit /b 1)
for %%O in (0 1 2 3) do (
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O%%O --no-artifact-cache -o "%DEST%\program-O%%O.exe" > "%DEST%\link-O%%O.log" 2>&1
  if errorlevel 1 (echo Enlace TLS O%%O fallo. & exit /b 1)
  "%DEST%\program-O%%O.exe"
  if errorlevel 1 (echo Aislamiento TLS O%%O fallo. & exit /b 1)
  echo Aislamiento TLS entre dos hilos O%%O: correcto.
)
exit /b 0
