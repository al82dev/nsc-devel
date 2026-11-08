@echo off
setlocal
set "DEST=A:\nsc-interop-build\t08-tls-dll-transitiva"
set "PATH=A:\al82dev-clang\bin;A:\nsc-interop-build\tanda1-ON\bin;%PATH%"
set "NSC_LINKER=A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
if not exist "%DEST%" mkdir "%DEST%"
if not exist "%DEST%\salida" mkdir "%DEST%\salida"
for %%F in (Consumer.exe api.dll api2.dll leaf.dll) do if exist "%DEST%\salida\%%F" del "%DEST%\salida\%%F"
for %%F in (Producer.ns Producer.nsproj Producer2.ns ReadApi.h) do copy /y "%~dp0..\tls-dll-distintas\%%F" "%DEST%\%%F" >nul || exit /b 1
copy /y "%~dp0..\tls-dll-distintas\Consumer2.ns" "%DEST%\Consumer.ns" >nul || exit /b 1
copy /y "%~dp0..\tls-dll-distintas\Consumer2.nsproj" "%DEST%\Consumer.nsproj" >nul || exit /b 1
for %%F in (TlsApi.h TlsApi.c ThreadApi.h ThreadApi.c) do copy /y "%~dp0..\..\pruebas-tanda-08-tls-c\directo\%%F" "%DEST%\%%F" >nul || exit /b 1
for %%F in (Leaf.c Leaf.h TlsApi2.c Producer2.nsproj) do copy /y "%~dp0%%F" "%DEST%\%%F" >nul || exit /b 1
pushd "%DEST%" || exit /b 1
for %%F in (TlsApi ThreadApi Leaf TlsApi2) do (
  clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 %%F.c -o %%F.obj > %%F.log 2>&1
  if errorlevel 1 (type %%F.log & popd & exit /b 1)
)
lld-link.exe /nologo /dll /noentry /nodefaultlib /out:leaf.dll /implib:leaf.lib Leaf.obj > leaf-link.log 2>&1
if errorlevel 1 (type leaf-link.log & popd & exit /b 1)
lld-link.exe /nologo /dll /noentry /nodefaultlib /include:_tls_used /export:tls_exported,DATA /out:api.dll /implib:api.lib TlsApi.obj ThreadApi.obj "A:\vc2026-runtime\lib\x64\release\libcmt.lib" "A:\vc2026-runtime\lib\x64\release\libcpmt.lib" "A:\vc2026-runtime\lib\x64\release\libvcruntime.lib" "A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\ucrt\x64\libucrt.lib" "A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\um\x64\kernel32.lib" > api-link.log 2>&1
if errorlevel 1 (type api-link.log & popd & exit /b 1)
lld-link.exe /nologo /dll /noentry /nodefaultlib /include:_tls_used /export:tls_exported,DATA /out:api2.dll /implib:api2.lib TlsApi2.obj leaf.lib "A:\vc2026-runtime\lib\x64\release\libcmt.lib" "A:\vc2026-runtime\lib\x64\release\libcpmt.lib" "A:\vc2026-runtime\lib\x64\release\libvcruntime.lib" "A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\ucrt\x64\libucrt.lib" "A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\um\x64\kernel32.lib" > api2-link.log 2>&1
if errorlevel 1 (type api2-link.log & popd & exit /b 1)
llvm-readobj.exe --coff-imports api2.dll > api2-imports.log 2>&1
findstr /c:"leaf.dll" api2-imports.log >nul
if errorlevel 1 (echo ERROR: api2.dll no importa leaf.dll. & popd & exit /b 1)
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-lib Producer.nsproj -O0 --no-artifact-cache -o TlsProducer.lib > producer.log 2>&1
if errorlevel 1 (type producer.log & popd & exit /b 1)
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-lib Producer2.nsproj -O0 --no-artifact-cache -o TlsProducer2.lib > producer2.log 2>&1
if errorlevel 1 (type producer2.log & popd & exit /b 1)
del TlsApi.h
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe Consumer.nsproj -O0 --no-artifact-cache -o salida\Consumer.exe > consumer.log 2>&1
if errorlevel 1 (type consumer.log & popd & exit /b 1)
for %%F in (api.dll api2.dll leaf.dll) do if not exist salida\%%F (echo ERROR: falta %%F en la salida aislada. & popd & exit /b 1)
pushd salida || (popd & exit /b 1)
Consumer.exe
if errorlevel 1 (echo ERROR: ejecucion con dependencia transitiva fallo. & popd & popd & exit /b 1)
popd
popd
echo TLS dinamica con dependencia transitiva: correcto.
exit /b 0
