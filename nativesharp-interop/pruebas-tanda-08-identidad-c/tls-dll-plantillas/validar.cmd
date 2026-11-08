@echo off
setlocal
if not defined NSC_T08_OPT set "NSC_T08_OPT=0"
set "DEST=A:\nsc-interop-build\t08-tls-dll-plantilla-repro"
if "%~1"=="dos" set "DEST=A:\nsc-interop-build\t08-tls-dll-dos-plantillas"
set "PATH=A:\al82dev-clang\bin;A:\nsc-interop-build\tanda1-ON\bin;%PATH%"
set "NSC_LINKER=A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
if not exist "%DEST%" mkdir "%DEST%"
for %%F in (Producer.ns Producer.nsproj Consumer.ns Consumer.nsproj ReadApi.h) do copy /y "%~dp0%%F" "%DEST%\%%F" >nul || exit /b 1
if "%~1"=="dos" (
  for %%F in (Producer2.ns Producer2.nsproj) do copy /y "%~dp0%%F" "%DEST%\%%F" >nul || exit /b 1
  copy /y "%~dp0Consumer2.ns" "%DEST%\Consumer.ns" >nul || exit /b 1
  copy /y "%~dp0Consumer2.nsproj" "%DEST%\Consumer.nsproj" >nul || exit /b 1
)
for %%F in (TlsApi.h TlsApi.c ThreadApi.h ThreadApi.c) do copy /y "%~dp0..\..\pruebas-tanda-08-tls-c\directo\%%F" "%DEST%\%%F" >nul || exit /b 1
pushd "%DEST%" || exit /b 1
for %%F in (TlsApi ThreadApi) do (
  clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 %%F.c -o %%F.obj > %%F.log 2>&1
  if errorlevel 1 (type %%F.log & popd & exit /b 1)
)
lld-link.exe /nologo /dll /noentry /nodefaultlib /include:_tls_used /export:tls_exported,DATA /out:api.dll /implib:api.lib TlsApi.obj ThreadApi.obj "A:\vc2026-runtime\lib\x64\release\libcmt.lib" "A:\vc2026-runtime\lib\x64\release\libcpmt.lib" "A:\vc2026-runtime\lib\x64\release\libvcruntime.lib" "A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\ucrt\x64\libucrt.lib" "A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\um\x64\kernel32.lib" > dll-link.log 2>&1
if errorlevel 1 (type dll-link.log & popd & exit /b 1)
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-lib Producer.nsproj -O%NSC_T08_OPT% --no-artifact-cache -o TlsProducer.lib > producer.log 2>&1
if errorlevel 1 (type producer.log & popd & exit /b 1)
if "%~1"=="dos" (
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-lib Producer2.nsproj -O%NSC_T08_OPT% --no-artifact-cache -o TlsProducer2.lib > producer2.log 2>&1
  if errorlevel 1 (type producer2.log & popd & exit /b 1)
)
del TlsApi.h
if exist TlsApi.h (popd & exit /b 1)
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe Consumer.nsproj -O%NSC_T08_OPT% --no-artifact-cache -o Consumer.exe > consumer.log 2>&1
if errorlevel 1 (type consumer.log & popd & exit /b 1)
Consumer.exe
if errorlevel 1 (popd & exit /b 1)
popd
echo TLS DLL en plantilla y otro hilo: correcto.
