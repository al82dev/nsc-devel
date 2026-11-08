@echo off
setlocal
set "DEST=A:\nsc-interop-build\t08-tls-interna-plantillas-repro"
if "%~1"=="hilos" set "DEST=A:\nsc-interop-build\t08-tls-interna-plantillas-hilos"
set "PATH=A:\nsc-interop-build\tanda1-ON\bin;A:\al82dev-clang\bin;%PATH%"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
set "NSC_LINKER=A:\al82dev-clang\bin\lld-link.exe"
for %%D in (a b) do (
  if not exist "%DEST%\%%D" mkdir "%DEST%\%%D"
  for %%F in (Producer.ns Producer.nsproj) do (
    copy /y "%~dp0%%D\%%F" "%DEST%\%%D\%%F" >nul || exit /b 1
  )
)
> "%DEST%\a\same.h" echo static _Thread_local int internal_value = 11;
> "%DEST%\b\same.h" echo static _Thread_local int internal_value = 29;
copy /y "%~dp0Consumer.ns" "%DEST%\Consumer.ns" >nul || exit /b 1
copy /y "%~dp0Consumer-tls.nsproj" "%DEST%\Consumer.nsproj" >nul || exit /b 1
copy /y "%~dp0Crt.h" "%DEST%\Crt.h" >nul || exit /b 1
if "%~1"=="hilos" (
  copy /y "%~dp0Consumer-hilos.ns" "%DEST%\Consumer.ns" >nul || exit /b 1
  copy /y "%~dp0Consumer-hilos.nsproj" "%DEST%\Consumer.nsproj" >nul || exit /b 1
  copy /y "%~dp0Thread.h" "%DEST%\Thread.h" >nul || exit /b 1
  copy /y "%~dp0Thread.c" "%DEST%\Thread.c" >nul || exit /b 1
)
pushd "%DEST%" || exit /b 1
if "%~1"=="hilos" (
  clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 Thread.c -o thread.obj
  if errorlevel 1 (popd & exit /b 1)
  llvm-lib.exe /nologo /out:thread.lib thread.obj
  if errorlevel 1 (popd & exit /b 1)
)
for %%D in (a b) do (
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-lib "%%D\Producer.nsproj" -O0 --no-artifact-cache -o "%%D\InlineProducer%%D.lib" > "%%D\build.log" 2>&1
  if errorlevel 1 (type "%%D\build.log" & popd & exit /b 1)
)
del a\same.h b\same.h
if exist a\same.h (popd & exit /b 1)
if exist b\same.h (popd & exit /b 1)
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe Consumer.nsproj -O0 --no-artifact-cache -o Consumer.exe > Consumer.log 2>&1
if errorlevel 1 (type Consumer.log & popd & exit /b 1)
Consumer.exe
if errorlevel 1 (popd & exit /b 1)
popd
echo TLS interna de dos bibliotecas conserva estados independientes.
