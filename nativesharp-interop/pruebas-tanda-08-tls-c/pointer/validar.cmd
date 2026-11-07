@echo off
setlocal
set "ROOT=A:\nsc-interop-build\tanda08-tls-pointer"
set "PATH=A:\al82dev-clang\bin;A:\nsc-interop-build\tanda1-ON\bin;%PATH%"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
for %%M in (static dll) do (
  if not exist "%ROOT%\%%M" mkdir "%ROOT%\%%M"
  for %%F in (StaticApi.h DllApi.h Api.c Main.ns Crt.h Static.nsproj Dll.nsproj Control.c) do (
    copy /y "%~dp0%%F" "%ROOT%\%%M\%%F" >nul
    if errorlevel 1 exit /b 1
  )
)
set "TMP=%ROOT%\static"
set "TEMP=%TMP%"
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 "%ROOT%\static\Api.c" -o "%ROOT%\static\api.obj" > "%ROOT%\static\compile.log" 2>&1
if errorlevel 1 exit /b 1
llvm-lib.exe /nologo /out:"%ROOT%\static\api.lib" "%ROOT%\static\api.obj"
if errorlevel 1 exit /b 1
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 "%ROOT%\static\Control.c" -o "%ROOT%\static\control.obj" > "%ROOT%\static\control-compile.log" 2>&1
if errorlevel 1 exit /b 1
lld-link.exe /nologo /nodefaultlib /subsystem:console /entry:mainCRTStartup /include:_tls_used /out:"%ROOT%\static\control.exe" "%ROOT%\static\control.obj" "%ROOT%\static\api.lib" "A:\vc2026-runtime\lib\x64\release\libcmt.lib" "A:\vc2026-runtime\lib\x64\release\libcpmt.lib" "A:\vc2026-runtime\lib\x64\release\libvcruntime.lib" "A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\ucrt\x64\libucrt.lib" "A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\um\x64\kernel32.lib" > "%ROOT%\static\control-link.log" 2>&1
if errorlevel 1 exit /b 1
"%ROOT%\static\control.exe"
if errorlevel 1 (echo Control C de puntero TLS fallo. & exit /b 1)
echo Control C de puntero TLS correcto.
set "NSC_LINKER=A:\al82dev-clang\bin\lld-link.exe"
for %%O in (0 1 2 3) do (
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%ROOT%\static\Static.nsproj" -O%%O --no-artifact-cache -o "%ROOT%\static\native-O%%O.exe" > "%ROOT%\static\native-O%%O.log" 2>&1
  if errorlevel 1 (echo Compilacion de puntero TLS estatico O%%O fallo. & exit /b 1)
  "%ROOT%\static\native-O%%O.exe"
  if errorlevel 1 (echo Ejecucion de puntero TLS estatico O%%O fallo. & exit /b 1)
  echo Puntero TLS estatico O%%O correcto.
)
set "TMP=%ROOT%\dll"
set "TEMP=%TMP%"
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 -DBUILD_DLL "%ROOT%\dll\Api.c" -o "%ROOT%\dll\api.obj" > "%ROOT%\dll\compile.log" 2>&1
if errorlevel 1 exit /b 1
lld-link.exe /nologo /dll /nodefaultlib /entry:_DllMainCRTStartup /include:_tls_used /export:tls_pointer,DATA /export:tls_double_pointer,DATA /export:tls_readonly_pointer,DATA /export:tls_volatile_pointer,DATA /export:tls_volatile_slot,DATA /export:tls_const_slot,DATA /out:"%ROOT%\dll\api.dll" /implib:"%ROOT%\dll\api.lib" "%ROOT%\dll\api.obj" "A:\vc2026-runtime\lib\x64\release\libcmt.lib" "A:\vc2026-runtime\lib\x64\release\libcpmt.lib" "A:\vc2026-runtime\lib\x64\release\libvcruntime.lib" "A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\ucrt\x64\libucrt.lib" "A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\um\x64\kernel32.lib" > "%ROOT%\dll\link.log" 2>&1
if errorlevel 1 exit /b 1
set "NSC_LINKER=A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
for %%O in (0 1 2 3) do (
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%ROOT%\dll\Dll.nsproj" -O%%O --no-artifact-cache -o "%ROOT%\dll\native-O%%O.exe" > "%ROOT%\dll\native-O%%O.log" 2>&1
  if errorlevel 1 (echo Compilacion de puntero TLS DLL O%%O fallo. & exit /b 1)
  "%ROOT%\dll\native-O%%O.exe"
  if errorlevel 1 (echo Ejecucion de puntero TLS DLL O%%O fallo. & exit /b 1)
  echo Puntero TLS DLL O%%O correcto.
)
exit /b 0
