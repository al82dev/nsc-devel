@echo off
setlocal
call "%~dp0validar.cmd" dos || exit /b 1
set "DEST=A:\nsc-interop-build\t08-tls-dos-dll-repro"
set "PATH=A:\al82dev-clang\bin;A:\nsc-interop-build\tanda1-ON\bin;%PATH%"
set "NSC_LINKER=A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
if not exist "%DEST%\otra" mkdir "%DEST%\otra"
if not exist "%DEST%\uno" mkdir "%DEST%\uno"
if not exist "%DEST%\dos" mkdir "%DEST%\dos"
for %%D in (uno dos) do for %%F in (api.dll api.lib TlsProducer.lib TlsProducer.nsmeta TlsProducer2.lib TlsProducer2.nsmeta) do if exist "%DEST%\%%D\%%F" del "%DEST%\%%D\%%F"
lld-link.exe /nologo /dll /noentry /nodefaultlib /include:_tls_used /export:tls_exported,DATA /out:"%DEST%\otra\api.dll" /implib:"%DEST%\otra\api.lib" "%DEST%\TlsApi2.obj" "A:\vc2026-runtime\lib\x64\release\libcmt.lib" "A:\vc2026-runtime\lib\x64\release\libcpmt.lib" "A:\vc2026-runtime\lib\x64\release\libvcruntime.lib" "A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\ucrt\x64\libucrt.lib" "A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\um\x64\kernel32.lib" > "%DEST%\mismonombre-dll.log" 2>&1
if errorlevel 1 (type "%DEST%\mismonombre-dll.log" & exit /b 1)
copy /y "%~dp0Producer2MismoNombre.nsproj" "%DEST%\Producer2MismoNombre.nsproj" >nul || exit /b 1
copy /y "%~dp0ConsumerMismoNombre.nsproj" "%DEST%\ConsumerMismoNombre.nsproj" >nul || exit /b 1
copy /y "%~dp0ControlMismoNombre.c" "%DEST%\ControlMismoNombre.c" >nul || exit /b 1
copy /y "%~dp0..\..\pruebas-tanda-08-tls-c\directo\TlsApi.h" "%DEST%\TlsApi.h" >nul || exit /b 1
pushd "%DEST%" || exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-lib Producer.nsproj -O0 --no-artifact-cache -o uno\TlsProducer.lib > mismonombre-producer1.log 2>&1
if errorlevel 1 (type mismonombre-producer1.log & popd & exit /b 1)
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-lib Producer2MismoNombre.nsproj -O0 --no-artifact-cache -o dos\TlsProducer2.lib > mismonombre-producer2.log 2>&1
if errorlevel 1 (type mismonombre-producer2.log & popd & exit /b 1)
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 ControlMismoNombre.c -o ControlMismoNombre.obj > mismonombre-control.log 2>&1
if errorlevel 1 (type mismonombre-control.log & popd & exit /b 1)
lld-link.exe /nologo /entry:mainCRTStartup /subsystem:console /nodefaultlib /out:ControlMismoNombre.exe ControlMismoNombre.obj "A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\um\x64\kernel32.lib" >> mismonombre-control.log 2>&1
if errorlevel 1 (type mismonombre-control.log & popd & exit /b 1)
ControlMismoNombre.exe
if errorlevel 1 (echo ERROR: control C de DLL de igual nombre fallo. & popd & exit /b 1)
echo Control C de DLL de igual nombre: correcto.
del TlsApi.h
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe ConsumerMismoNombre.nsproj -O0 --no-artifact-cache -o ConsumerSame.exe > mismonombre-consumer.log 2>&1
if not errorlevel 1 (echo ERROR: se aceptaron DLL distintas con igual nombre. & popd & exit /b 1)
findstr /c:"same file name cannot yet be distinguished" mismonombre-consumer.log >nul
if errorlevel 1 (type mismonombre-consumer.log & popd & exit /b 1)
popd
echo DLL distintas con igual nombre: diagnostico detectado; interoperabilidad pendiente.
exit /b 0
