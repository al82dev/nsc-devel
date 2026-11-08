@echo off
setlocal
call "%~dp0validar.cmd" dos || exit /b 1
set "DEST=A:\nsc-interop-build\t08-tls-dos-dll-repro"
set "PATH=A:\al82dev-clang\bin;A:\nsc-interop-build\tanda1-ON\bin;%PATH%"
set "NSC_LINKER=A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
if not exist "%DEST%\otra" mkdir "%DEST%\otra"
lld-link.exe /nologo /dll /noentry /nodefaultlib /include:_tls_used /export:tls_exported,DATA /out:"%DEST%\otra\api.dll" /implib:"%DEST%\otra\api.lib" "%DEST%\TlsApi2.obj" "A:\vc2026-runtime\lib\x64\release\libcmt.lib" "A:\vc2026-runtime\lib\x64\release\libcpmt.lib" "A:\vc2026-runtime\lib\x64\release\libvcruntime.lib" "A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\ucrt\x64\libucrt.lib" "A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\um\x64\kernel32.lib" > "%DEST%\mismonombre-dll.log" 2>&1
if errorlevel 1 (type "%DEST%\mismonombre-dll.log" & exit /b 1)
copy /y "%~dp0Producer2MismoNombre.nsproj" "%DEST%\Producer2MismoNombre.nsproj" >nul || exit /b 1
copy /y "%~dp0..\..\pruebas-tanda-08-tls-c\directo\TlsApi.h" "%DEST%\TlsApi.h" >nul || exit /b 1
pushd "%DEST%" || exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-lib Producer2MismoNombre.nsproj -O0 --no-artifact-cache -o TlsProducer2.lib > mismonombre-producer.log 2>&1
if not errorlevel 1 (echo ERROR: se aceptaron DLL distintas con igual nombre. & popd & exit /b 1)
findstr /c:"runtime package destination already contains a different" mismonombre-producer.log >nul
if errorlevel 1 (type mismonombre-producer.log & popd & exit /b 1)
popd
echo DLL distintas con igual nombre: conflicto de empaquetado detectado; interoperabilidad pendiente.
exit /b 0
