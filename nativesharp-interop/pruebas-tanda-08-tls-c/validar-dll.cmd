@echo off
setlocal
set "DEST=A:\nsc-interop-build\tanda08-tls-dll"
set "PATH=A:\al82dev-clang\bin;A:\nsc-interop-build\tanda1-ON\bin;%PATH%"
if not exist "%DEST%" mkdir "%DEST%"
if errorlevel 1 exit /b 1
set "TMP=%DEST%"
set "TEMP=%DEST%"
set "NSC_LINKER=A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
for %%F in (Api.h Api.c ApiDll.h Main.ns ProjectDll.nsproj) do (
  copy /y "%~dp0%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 "%DEST%\Api.c" -o "%DEST%\api.obj" > "%DEST%\compile.log" 2>&1
if errorlevel 1 (echo Compilacion DLL TLS fallo. & exit /b 1)
lld-link.exe /nologo /dll /noentry /nodefaultlib /include:_tls_used /export:verify_tls_isolation /out:"%DEST%\api.dll" /implib:"%DEST%\api.lib" "%DEST%\api.obj" "A:\vc2026-runtime\lib\x64\release\libcmt.lib" "A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\um\x64\kernel32.lib" > "%DEST%\dll-link.log" 2>&1
if errorlevel 1 (echo Enlace DLL TLS fallo. & exit /b 1)
llvm-readobj.exe --coff-tls-directory "%DEST%\api.dll" > "%DEST%\tls-directory.txt" 2>&1
if errorlevel 1 (echo Inspeccion DLL TLS fallo. & exit /b 1)
findstr /i /c:"AddressOfIndex" "%DEST%\tls-directory.txt" >nul
if errorlevel 1 (echo No se encontro el directorio TLS. & exit /b 1)
for %%O in (0 1 2 3) do (
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\ProjectDll.nsproj" -O%%O --no-artifact-cache -o "%DEST%\program-O%%O.exe" > "%DEST%\link-O%%O.log" 2>&1
  if errorlevel 1 (echo Enlace NSC DLL TLS O%%O fallo. & exit /b 1)
  "%DEST%\program-O%%O.exe"
  if errorlevel 1 (echo Aislamiento DLL TLS O%%O fallo. & exit /b 1)
  echo Aislamiento DLL TLS O%%O correcto.
)
echo TLS DLL COMPLETO
exit /b 0
