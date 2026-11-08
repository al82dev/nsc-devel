@echo off
setlocal
if not defined NSC_T08_OPT set "NSC_T08_OPT=0"
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
copy /y "%~dp0ConsumerMismoNombreDirecto.ns" "%DEST%\ConsumerMismoNombreDirecto.ns" >nul || exit /b 1
copy /y "%~dp0ConsumerMismoNombreDinamico.nsproj" "%DEST%\ConsumerMismoNombreDinamico.nsproj" >nul || exit /b 1
copy /y "%~dp0ConsumerMismoNombreCache.nsproj" "%DEST%\ConsumerMismoNombreCache.nsproj" >nul || exit /b 1
copy /y "%~dp0ConsumerMismoNombreDinamico.ns" "%DEST%\ConsumerMismoNombreDinamico.ns" >nul || exit /b 1
copy /y "%~dp0ThreadOnly.h" "%DEST%\ThreadOnly.h" >nul || exit /b 1
copy /y "%~dp0ConsumerCache.ns" "%DEST%\ConsumerCache.ns" >nul || exit /b 1
copy /y "%~dp0ControlMismoNombre.c" "%DEST%\ControlMismoNombre.c" >nul || exit /b 1
copy /y "%~dp0..\..\pruebas-tanda-08-tls-c\directo\TlsApi.h" "%DEST%\TlsApi.h" >nul || exit /b 1
pushd "%DEST%" || exit /b 1
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 "%~dp0ThreadOnly.c" -o ThreadOnly.obj > thread-only.log 2>&1
if errorlevel 1 (type thread-only.log & popd & exit /b 1)
lld-link.exe /nologo /dll /noentry /nodefaultlib /out:thread-only.dll /implib:thread-only.lib ThreadOnly.obj "A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\um\x64\kernel32.lib" >> thread-only.log 2>&1
if errorlevel 1 (type thread-only.log & popd & exit /b 1)
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-lib Producer.nsproj -O%NSC_T08_OPT% --no-artifact-cache -o uno\TlsProducer.lib > mismonombre-producer1.log 2>&1
if errorlevel 1 (type mismonombre-producer1.log & popd & exit /b 1)
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-lib Producer2MismoNombre.nsproj -O%NSC_T08_OPT% --no-artifact-cache -o dos\TlsProducer2.lib > mismonombre-producer2.log 2>&1
if errorlevel 1 (type mismonombre-producer2.log & popd & exit /b 1)
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 ControlMismoNombre.c -o ControlMismoNombre.obj > mismonombre-control.log 2>&1
if errorlevel 1 (type mismonombre-control.log & popd & exit /b 1)
lld-link.exe /nologo /entry:mainCRTStartup /subsystem:console /nodefaultlib /out:ControlMismoNombre.exe ControlMismoNombre.obj "A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\um\x64\kernel32.lib" >> mismonombre-control.log 2>&1
if errorlevel 1 (type mismonombre-control.log & popd & exit /b 1)
ControlMismoNombre.exe
if errorlevel 1 (echo ERROR: control C de DLL de igual nombre fallo. & popd & exit /b 1)
pushd A:\ || (popd & exit /b 1)
"%DEST%\ControlMismoNombre.exe"
if errorlevel 1 (echo ERROR: control C desde otro directorio fallo. & popd & popd & exit /b 1)
popd
echo Control C de DLL de igual nombre: correcto.
copy /y "uno\api.dll" "alias-a.dll" >nul || (popd & exit /b 1)
copy /y "dos\api.dll" "alias-b.dll" >nul || (popd & exit /b 1)
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 "%~dp0ControlRenombrado.c" -o ControlRenombrado.obj > renombrado-control.log 2>&1
if errorlevel 1 (type renombrado-control.log & popd & exit /b 1)
lld-link.exe /nologo /entry:mainCRTStartup /subsystem:console /nodefaultlib /out:ControlRenombrado.exe ControlRenombrado.obj "A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\um\x64\kernel32.lib" >> renombrado-control.log 2>&1
if errorlevel 1 (type renombrado-control.log & popd & exit /b 1)
pushd A:\ || (popd & exit /b 1)
"%DEST%\ControlRenombrado.exe"
if errorlevel 1 (echo ERROR: control C de DLL renombradas fallo. & popd & popd & exit /b 1)
popd
echo Control C de DLL renombradas: correcto.
call "%~dp0diagnosticar-import-directo.cmd"
if errorlevel 1 (popd & exit /b 1)
del TlsApi.h
if not exist salida-mismo-nombre mkdir salida-mismo-nombre
if exist salida-mismo-nombre\ConsumerSameDynamic.exe del salida-mismo-nombre\ConsumerSameDynamic.exe
if exist salida-mismo-nombre\__nsc_tls_*.dll del salida-mismo-nombre\__nsc_tls_*.dll
if exist salida-mismo-nombre\thread-only.dll del salida-mismo-nombre\thread-only.dll
copy /y dos\api.dll salida-mismo-nombre\api.dll >nul || (popd & exit /b 1)
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe ConsumerMismoNombreDinamico.nsproj -O%NSC_T08_OPT% --no-artifact-cache -o salida-mismo-nombre\ConsumerSameDynamic.exe > mismonombre-dinamico.log 2>&1
if errorlevel 1 (type mismonombre-dinamico.log & popd & exit /b 1)
fc /b dos\api.dll salida-mismo-nombre\api.dll >nul
if errorlevel 1 (echo ERROR: se modifico la DLL preexistente. & popd & exit /b 1)
pushd A:\ || (popd & exit /b 1)
"%DEST%\salida-mismo-nombre\ConsumerSameDynamic.exe"
if errorlevel 1 (echo ERROR: consumidor de DLL homonimas fallo. & popd & popd & exit /b 1)
popd
echo Consumidor de DLL homonimas: correcto.
if defined NSC_T08_CACHE (
  call "%~dp0validar-mismo-nombre-cache.cmd"
  if errorlevel 1 (popd & exit /b 1)
)
if not exist traslado-directo mkdir traslado-directo
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe ConsumerMismoNombre.nsproj -O%NSC_T08_OPT% --no-artifact-cache -o traslado-directo\ConsumerSame.exe > mismonombre-consumer.log 2>&1
if errorlevel 1 (type mismonombre-consumer.log & popd & exit /b 1)
llvm-readobj.exe --coff-imports traslado-directo\ConsumerSame.exe > traslado-directo\imports.log 2>&1
if errorlevel 1 (type traslado-directo\imports.log & popd & exit /b 1)
findstr /c:"Name: __nsc_tls_" traslado-directo\imports.log >nul || (echo ERROR: falta la importacion por alias. & popd & exit /b 1)
findstr /c:"Symbol: tls_read" traslado-directo\imports.log >nul || (echo ERROR: falta la funcion C importada. & popd & exit /b 1)
findstr /i /c:"Name: api.dll" traslado-directo\imports.log >nul && (echo ERROR: se conserva la importacion homonima. & popd & exit /b 1)
if not exist traslado-directo-copia mkdir traslado-directo-copia
if exist traslado-directo-copia\api.dll (echo ERROR: el destino de traslado contiene api.dll. & popd & exit /b 1)
copy /y traslado-directo\ConsumerSame.exe traslado-directo-copia\ConsumerSame.exe >nul || (popd & exit /b 1)
copy /y traslado-directo\__nsc_tls_*.dll traslado-directo-copia\ >nul || (popd & exit /b 1)
pushd A:\ || (popd & exit /b 1)
"%DEST%\traslado-directo-copia\ConsumerSame.exe"
if errorlevel 1 (echo ERROR: importacion directa y TLS no comparten instancia. & popd & popd & exit /b 1)
popd
if not defined NSC_T08_CACHE goto :sin_cache_directo
python "%~dp0validar-directo-precompilado.py"
if errorlevel 1 (popd & exit /b 1)
:sin_cache_directo
popd
echo Importacion directa y TLS tras traslado: correcto.
exit /b 0
