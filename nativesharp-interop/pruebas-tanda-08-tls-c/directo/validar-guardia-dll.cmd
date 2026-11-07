@echo off
setlocal
set "DEST=A:\nsc-interop-build\tanda08-tls-directo"
set "PATH=A:\al82dev-clang\bin;A:\nsc-interop-build\tanda1-ON\bin;%PATH%"
if not exist "%DEST%" mkdir "%DEST%"
set "TMP=%DEST%"
set "TEMP=%DEST%"
for %%F in (TlsApi.h Native.ns Project.nsproj) do (
  copy /y "%~dp0%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
nsc.exe --cpp-clang-root=A:\al82dev-clang --check-project-method-bodies "%DEST%\Project.nsproj" > "%DEST%\guardia-dll.log" 2>&1
if not errorlevel 1 (echo TLS de DLL fue aceptado sin soporte. & exit /b 1)
findstr /c:"thread-local C/C++ global from a runtime library is not yet supported" "%DEST%\guardia-dll.log" >nul
if errorlevel 1 (echo Falta el diagnostico esperado. & exit /b 1)
echo TLS directo de DLL identificado y rechazado mientras no exista soporte.
exit /b 0
