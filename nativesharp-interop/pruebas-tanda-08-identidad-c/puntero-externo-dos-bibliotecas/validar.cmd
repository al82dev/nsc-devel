@echo off
setlocal
set "DEST=A:\nsc-interop-build\t08-puntero-externo-%~1"
set "PATH=A:\nsc-interop-build\tanda1-ON\bin;A:\al82dev-clang\bin;%PATH%"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
set "NSC_LINKER=A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
for %%D in (a b) do (
  if not exist "%DEST%\%%D" mkdir "%DEST%\%%D"
  for %%F in (same.h Producer.ns Producer.nsproj) do (
    copy /y "%~dp0%%D\%%F" "%DEST%\%%D\%%F" >nul || exit /b 1
  )
)
for %%F in (fixture.c Consumer.ns Consumer.nsproj) do (
  copy /y "%~dp0%%F" "%DEST%\%%F" >nul || exit /b 1
)
if "%~1"=="const" > "%DEST%\b\same.h" echo extern const int *external_pointer;
if "%~1"=="volatile" > "%DEST%\b\same.h" echo extern volatile int *external_pointer;
pushd "%DEST%" || exit /b 1
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 fixture.c -o fixture.obj
if errorlevel 1 (popd & exit /b 1)
llvm-lib.exe /nologo /out:fixture.lib fixture.obj
if errorlevel 1 (popd & exit /b 1)
for %%D in (a b) do (
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-lib "%%D\Producer.nsproj" -O0 --no-artifact-cache -o "%%D\InlineProducer%%D.lib" > "%%D\build.log" 2>&1
  if errorlevel 1 (type "%%D\build.log" & popd & exit /b 1)
)
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe Consumer.nsproj -O0 --no-artifact-cache -o Consumer.exe > Consumer.log 2>&1
if "%~1"=="const" goto expect_incompatible
if "%~1"=="volatile" goto expect_incompatible
if errorlevel 1 (type Consumer.log & popd & exit /b 1)
Consumer.exe
if errorlevel 1 (popd & exit /b 1)
popd
echo Puntero externo compartido validado.
exit /b 0
:expect_incompatible
if not errorlevel 1 (echo Se aceptaron declaraciones incompatibles. & popd & exit /b 1)
findstr /c:"ambiguous namespace state reference" Consumer.log >nul
if errorlevel 1 (type Consumer.log & popd & exit /b 1)
popd
echo Calificadores incompatibles rechazados.
exit /b 0
