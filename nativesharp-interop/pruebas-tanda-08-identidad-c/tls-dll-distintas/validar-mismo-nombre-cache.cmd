@echo off
setlocal
if exist salida-mismo-nombre\ConsumerSameDynamic.exe del salida-mismo-nombre\ConsumerSameDynamic.exe
if exist salida-mismo-nombre\__nsc_tls_*.dll del salida-mismo-nombre\__nsc_tls_*.dll
for %%I in (1 2) do (
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe ConsumerMismoNombreCache.nsproj -O%NSC_T08_OPT% --time --artifact-cache=A:\nsc-interop-build\t08-tls-mismo-nombre-cache -o salida-mismo-nombre\ConsumerSameDynamic.exe > mismonombre-cache-%%I.log 2>&1
  if errorlevel 1 (type mismonombre-cache-%%I.log & exit /b 1)
  pushd A:\ || exit /b 1
  "%DEST%\salida-mismo-nombre\ConsumerSameDynamic.exe"
  if errorlevel 1 (popd & echo ERROR: ejecucion con cache fallo. & exit /b 1)
  popd
  if %%I==1 (
    del salida-mismo-nombre\ConsumerSameDynamic.exe
    del salida-mismo-nombre\__nsc_tls_*.dll
  )
)
findstr /c:"compilation-bundle-cache: hit" mismonombre-cache-2.log >nul
if errorlevel 1 (type mismonombre-cache-2.log & echo ERROR: no se restauro desde cache. & exit /b 1)
set "NSC_T08_PREEXIST_CACHE=A:\nsc-interop-build\t08-tls-preexist-cache\%RANDOM%-%RANDOM%"
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe ConsumerMismoNombreCache.nsproj -O%NSC_T08_OPT% --time --artifact-cache=%NSC_T08_PREEXIST_CACHE% -o salida-mismo-nombre\ConsumerSameDynamic.exe > mismonombre-preexist-1.log 2>&1
if errorlevel 1 (type mismonombre-preexist-1.log & exit /b 1)
findstr /c:"compilation-bundle-cache: not-found" mismonombre-preexist-1.log >nul
if errorlevel 1 (type mismonombre-preexist-1.log & echo ERROR: se esperaba cache nueva. & exit /b 1)
del salida-mismo-nombre\ConsumerSameDynamic.exe
del salida-mismo-nombre\__nsc_tls_*.dll
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe ConsumerMismoNombreCache.nsproj -O%NSC_T08_OPT% --time --artifact-cache=%NSC_T08_PREEXIST_CACHE% -o salida-mismo-nombre\ConsumerSameDynamic.exe > mismonombre-preexist-2.log 2>&1
if errorlevel 1 (type mismonombre-preexist-2.log & exit /b 1)
findstr /c:"compilation-bundle-cache: hit" mismonombre-preexist-2.log >nul
if errorlevel 1 (type mismonombre-preexist-2.log & echo ERROR: no se restauro cache con DLL preexistentes. & exit /b 1)
pushd A:\ || exit /b 1
"%DEST%\salida-mismo-nombre\ConsumerSameDynamic.exe"
if errorlevel 1 (popd & echo ERROR: no se restauraron las DLL preexistentes. & exit /b 1)
popd
echo Cache de DLL homonimas: correcta.
exit /b 0
