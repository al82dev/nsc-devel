@echo off
setlocal
set "DEST=A:\nsc-interop-build\tanda08-seleccion-lld"
set "PATH=A:\al82dev-clang\bin;A:\nsc-interop-build\tanda1-ON\bin;%PATH%"
set "TMP=%DEST%"
set "TEMP=%DEST%"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
if not exist "%DEST%" mkdir "%DEST%"
if errorlevel 1 exit /b 1
copy /y "%~dp0..\Project.nsproj" "%DEST%\Project.nsproj" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0..\Main.ns" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0..\Api.h" "%DEST%\Api.h" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0..\Api.c" "%DEST%\Api.c" >nul
if errorlevel 1 exit /b 1
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 "%DEST%\Api.c" -o "%DEST%\api.obj"
if errorlevel 1 exit /b 1
llvm-lib.exe /nologo /out:"%DEST%\api.lib" "%DEST%\api.obj"
if errorlevel 1 exit /b 1
set "NSC_LINKER=A:\al82dev-clang\bin\lld-link.exe"
nsc.exe --print-linker > "%DEST%\lld.log" 2>&1
if errorlevel 1 exit /b 1
findstr /L /C:"lld-link.exe" "%DEST%\lld.log" >nul
if errorlevel 1 exit /b 1
nsc.exe build "%DEST%\Project.nsproj" --no-restore > "%DEST%\build.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /L /C:"project build does not yet support the lld-link selection" "%DEST%\build.log" >nul
if errorlevel 1 exit /b 1
set "NSC_LINKER=A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
nsc.exe --print-linker > "%DEST%\nslink.log" 2>&1
if errorlevel 1 exit /b 1
findstr /L /C:"nslink.exe" "%DEST%\nslink.log" >nul
if errorlevel 1 exit /b 1
set "NSC_LINKER=other-linker.exe"
nsc.exe --print-linker > "%DEST%\invalid.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /L /C:"nslink.exe or lld-link.exe" "%DEST%\invalid.log" >nul
if errorlevel 1 exit /b 1
echo Seleccion explicita de LLD, NSLink y rechazo de herramienta ajena: correcto.
exit /b 0
