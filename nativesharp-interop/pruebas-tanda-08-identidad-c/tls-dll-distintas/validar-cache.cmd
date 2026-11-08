@echo off
setlocal
call "%~dp0validar.cmd" dos || exit /b 1
set "DEST=A:\nsc-interop-build\t08-tls-dos-dll-repro"
set "PATH=A:\al82dev-clang\bin;A:\nsc-interop-build\tanda1-ON\bin;%PATH%"
set "NSC_LINKER=A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
for %%F in (ConsumerCache.ns ConsumerCache.nsproj) do copy /y "%~dp0%%F" "%DEST%\%%F" >nul || exit /b 1
if not exist "%DEST%\salida-cache" mkdir "%DEST%\salida-cache"
for %%F in (ConsumerCache.exe api.dll api2.dll) do if exist "%DEST%\salida-cache\%%F" del "%DEST%\salida-cache\%%F"
pushd "%DEST%" || exit /b 1
for %%I in (1 2) do (
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe ConsumerCache.nsproj -O0 --time --artifact-cache=A:\nsc-interop-build\t08-tls-cache -o salida-cache\ConsumerCache.exe > cache-%%I.log 2>&1
  if errorlevel 1 (type cache-%%I.log & popd & exit /b 1)
  for %%F in (ConsumerCache.exe api.dll api2.dll) do if not exist salida-cache\%%F (echo ERROR: falta %%F en salida cacheada. & popd & exit /b 1)
  pushd salida-cache || (popd & exit /b 1)
  ConsumerCache.exe
  if errorlevel 1 (echo ERROR: ejecucion TLS con cache fallo. & popd & popd & exit /b 1)
  popd
  if %%I==1 for %%F in (ConsumerCache.exe api.dll api2.dll) do del salida-cache\%%F
)
findstr /c:"compilation-bundle-cache: hit" cache-2.log >nul
if errorlevel 1 (type cache-2.log & echo ERROR: no se observo reutilizacion de cache. & popd & exit /b 1)
popd
echo Cache de consumidor con dos DLL TLS: correcta.
exit /b 0
