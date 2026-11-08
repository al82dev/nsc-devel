@echo off
setlocal
set "DEST=A:\nsc-interop-build\t08-tls-dll-concurrencia"
set "PATH=A:\al82dev-clang\bin;A:\nsc-interop-build\tanda1-ON\bin;%PATH%"
set "NSC_LINKER=A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
if not exist "%DEST%" mkdir "%DEST%"
for %%F in (Producer.ns Producer.nsproj Producer2.ns Producer2.nsproj TlsApi2.c) do copy /y "%~dp0..\tls-dll-distintas\%%F" "%DEST%\%%F" >nul || exit /b 1
for %%F in (TlsApi.h TlsApi.c) do copy /y "%~dp0..\..\pruebas-tanda-08-tls-c\directo\%%F" "%DEST%\%%F" >nul || exit /b 1
for %%F in (RaceApi.h RaceApi.c LoadHook.c Consumer.ns Consumer.nsproj) do copy /y "%~dp0%%F" "%DEST%\%%F" >nul || exit /b 1
pushd "%DEST%" || exit /b 1
for %%F in (TlsApi RaceApi LoadHook TlsApi2) do (
  clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 %%F.c -o %%F.obj > %%F.log 2>&1
  if errorlevel 1 (type %%F.log & popd & exit /b 1)
)
lld-link.exe /nologo /dll /noentry /nodefaultlib /include:_tls_used /export:tls_exported,DATA /out:api.dll /implib:api.lib TlsApi.obj RaceApi.obj LoadHook.obj "A:\vc2026-runtime\lib\x64\release\libcmt.lib" "A:\vc2026-runtime\lib\x64\release\libcpmt.lib" "A:\vc2026-runtime\lib\x64\release\libvcruntime.lib" "A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\ucrt\x64\libucrt.lib" "A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\um\x64\kernel32.lib" > api-link.log 2>&1
if errorlevel 1 (type api-link.log & popd & exit /b 1)
lld-link.exe /nologo /dll /noentry /nodefaultlib /include:_tls_used /export:tls_exported,DATA /out:api2.dll /implib:api2.lib TlsApi2.obj "A:\vc2026-runtime\lib\x64\release\libcmt.lib" "A:\vc2026-runtime\lib\x64\release\libcpmt.lib" "A:\vc2026-runtime\lib\x64\release\libvcruntime.lib" "A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\ucrt\x64\libucrt.lib" "A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\um\x64\kernel32.lib" > api2-link.log 2>&1
if errorlevel 1 (type api2-link.log & popd & exit /b 1)
for %%O in (0 1 2 3) do (
  copy /y "%~dp0..\..\pruebas-tanda-08-tls-c\directo\TlsApi.h" TlsApi.h >nul || (popd & exit /b 1)
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-lib Producer.nsproj -O%%O --no-artifact-cache -o TlsProducer.lib > producer.log 2>&1
  if errorlevel 1 (type producer.log & popd & exit /b 1)
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-lib Producer2.nsproj -O%%O --no-artifact-cache -o TlsProducer2.lib > producer2.log 2>&1
  if errorlevel 1 (type producer2.log & popd & exit /b 1)
  del TlsApi.h
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe Consumer.nsproj -O%%O --no-artifact-cache -o Consumer.exe > consumer.log 2>&1
  if errorlevel 1 (type consumer.log & popd & exit /b 1)
  llvm-readobj.exe --coff-imports Consumer.exe > imports.log 2>&1
  findstr /c:"Name: api2.dll" imports.log >nul
  if not errorlevel 1 (echo ERROR: api2.dll se cargo mediante importacion estatica. & popd & exit /b 1)
  for %%I in (1 2 3 4 5) do (
    Consumer.exe
    if errorlevel 1 (echo ERROR: primeros accesos TLS concurrentes fallaron. & popd & exit /b 1)
  )
  echo Dos hilos dentro del control del cargador: O%%O correcto en cinco procesos.
)
popd
exit /b 0
