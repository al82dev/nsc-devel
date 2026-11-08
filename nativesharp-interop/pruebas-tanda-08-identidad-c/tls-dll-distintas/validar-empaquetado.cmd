@echo off
setlocal
call "%~dp0validar.cmd" dos || exit /b 1
set "DEST=A:\nsc-interop-build\t08-tls-dos-dll-repro"
set "PATH=A:\al82dev-clang\bin;A:\nsc-interop-build\tanda1-ON\bin;%PATH%"
set "NSC_LINKER=A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
if not exist "%DEST%\salida-aislada" mkdir "%DEST%\salida-aislada"
for %%F in (Consumer.exe api.dll api2.dll) do if exist "%DEST%\salida-aislada\%%F" del "%DEST%\salida-aislada\%%F"
pushd "%DEST%" || exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe Consumer.nsproj -O0 --no-artifact-cache -o salida-aislada\Consumer.exe > empaquetado.log 2>&1
if errorlevel 1 (type empaquetado.log & popd & exit /b 1)
if not exist salida-aislada\api.dll (echo ERROR: falta api.dll en salida aislada. & popd & exit /b 1)
if not exist salida-aislada\api2.dll (echo ERROR: falta api2.dll en salida aislada. & popd & exit /b 1)
pushd salida-aislada || (popd & exit /b 1)
Consumer.exe
if errorlevel 1 (echo ERROR: ejecucion aislada de dos DLL fallo. & popd & popd & exit /b 1)
popd
pushd A:\ || (popd & exit /b 1)
"%DEST%\salida-aislada\Consumer.exe"
if errorlevel 1 (echo ERROR: ejecucion desde otro directorio fallo. & popd & popd & exit /b 1)
popd
popd
echo Empaquetado y ejecucion aislados: correcto.
exit /b 0
