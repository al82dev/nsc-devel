@echo off
setlocal
set "DEST=A:\nsc-interop-build\tanda08-lld-cache\run-%RANDOM%-%RANDOM%"
if exist "%DEST%" exit /b 1
mkdir "%DEST%"
if errorlevel 1 exit /b 1
set "PATH=A:\al82dev-clang\bin;A:\nsc-interop-build\tanda1-ON\bin;%PATH%"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
set "TMP=%DEST%"
set "TEMP=%DEST%"
for %%F in (Plain.ns Plain.nsproj) do (
  copy /y "%~dp0%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
set "NSC_LINKER=A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
nsc.exe --verbose --emit-project-oop-exe "%DEST%\Plain.nsproj" --nsc-linker-selection=NSLink --incremental-link --no-artifact-cache -O0 -o "%DEST%\plain.exe" > "%DEST%\nslink.log" 2>&1
if errorlevel 1 (type "%DEST%\nslink.log" & exit /b 1)
"%DEST%\plain.exe"
if errorlevel 1 exit /b 1
set "NSC_LINKER="
for %%I in (1 2) do (
  nsc.exe --verbose --emit-project-oop-exe "%DEST%\Plain.nsproj" --nsc-linker-selection=LLD --incremental-link --no-artifact-cache -O0 -o "%DEST%\plain.exe" > "%DEST%\lld-%%I.log" 2>&1
  if errorlevel 1 (type "%DEST%\lld-%%I.log" & exit /b 1)
)
findstr /L /C:"incremental-link: miss" "%DEST%\lld-1.log" >nul
if errorlevel 1 (type "%DEST%\lld-1.log" & exit /b 1)
findstr /L /C:"incremental-link: hit" "%DEST%\lld-2.log" >nul
if errorlevel 1 (type "%DEST%\lld-2.log" & exit /b 1)
"%DEST%\plain.exe"
if errorlevel 1 exit /b 1
echo Cambio NSLink a LLD invalida cache y repeticion LLD reutiliza: correcto.
set "PATH=A:\nsc-interop-build\tanda1-ON\bin;C:\Windows\System32"
nsc.exe --emit-project-oop-exe "%DEST%\Plain.nsproj" --nsc-linker-selection=LLD --no-artifact-cache -O0 -o "%DEST%\missing.exe" > "%DEST%\missing.log" 2>&1
if not errorlevel 1 (type "%DEST%\missing.log" & exit /b 1)
findstr /L /C:"not found in PATH" "%DEST%\missing.log" >nul
if errorlevel 1 (type "%DEST%\missing.log" & exit /b 1)
if exist "%DEST%\missing.exe" exit /b 1
echo LLD ausente produce diagnostico y no deja ejecutable: correcto.
exit /b 0
