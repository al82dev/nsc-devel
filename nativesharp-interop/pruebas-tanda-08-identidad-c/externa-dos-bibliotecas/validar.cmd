@echo off
setlocal
set "DEST=A:\nsc-interop-build\t08-global-externa-dos-gate"
if "%~1"=="incompatible" set "DEST=A:\nsc-interop-build\t08-global-externa-dos-incompatible"
if "%~1"=="volatile" set "DEST=A:\nsc-interop-build\t08-global-externa-dos-volatile"
if "%~1"=="volatile-compatible" set "DEST=A:\nsc-interop-build\t08-global-externa-dos-volatile-compatible"
if "%~1"=="const" set "DEST=A:\nsc-interop-build\t08-global-externa-dos-const"
if "%~1"=="const-compatible" set "DEST=A:\nsc-interop-build\t08-global-externa-dos-const-compatible"
if "%~1"=="atomic" set "DEST=A:\nsc-interop-build\t08-global-externa-dos-atomic"
if "%~1"=="atomic-compatible" set "DEST=A:\nsc-interop-build\t08-global-externa-dos-atomic-compatible"
set "PATH=A:\nsc-interop-build\tanda1-ON\bin;A:\al82dev-clang\bin;%PATH%"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
set "NSC_LINKER=A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
for %%D in (a b) do (
  if not exist "%DEST%\%%D" mkdir "%DEST%\%%D"
  for %%F in (same.h Producer.ns Producer.nsproj) do (
    copy /y "%~dp0%%D\%%F" "%DEST%\%%D\%%F" >nul
    if errorlevel 1 exit /b 1
  )
)
for %%F in (fixture.c Consumer.ns Consumer.nsproj) do (
  copy /y "%~dp0%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
if "%~1"=="incompatible" > "%DEST%\b\same.h" echo extern long internal_value;
if "%~1"=="volatile" > "%DEST%\b\same.h" echo extern volatile int internal_value;
if "%~1"=="volatile-compatible" (
  > "%DEST%\a\same.h" echo extern volatile int internal_value;
  > "%DEST%\b\same.h" echo extern volatile int internal_value;
  > "%DEST%\fixture.c" echo volatile int internal_value = 11;
)
if "%~1"=="const" goto setup_const
if "%~1"=="const-compatible" goto setup_const
if "%~1"=="atomic" goto setup_atomic
if "%~1"=="atomic-compatible" goto setup_atomic
goto build_fixture
:setup_const
for %%D in (a b) do (
  copy /y "%~dp0%%D\ReadOnly.ns" "%DEST%\%%D\Producer.ns" >nul || exit /b 1
)
copy /y "%~dp0Consumer-readonly.ns" "%DEST%\Consumer.ns" >nul || exit /b 1
> "%DEST%\b\same.h" echo extern const int internal_value;
if "%~1"=="const-compatible" (
  > "%DEST%\a\same.h" echo extern const int internal_value;
  > "%DEST%\fixture.c" echo const int internal_value = 11;
)
goto build_fixture
:setup_atomic
> "%DEST%\a\same.h" echo extern _Atomic(int) internal_value;
> "%DEST%\fixture.c" echo _Atomic(int) internal_value = 11;
if "%~1"=="atomic-compatible" > "%DEST%\b\same.h" echo extern _Atomic(int) internal_value;
:build_fixture
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
if "%~1"=="incompatible" goto expect_incompatible
if "%~1"=="volatile" goto expect_incompatible
if "%~1"=="const" goto expect_incompatible
if "%~1"=="atomic" goto expect_incompatible
if errorlevel 1 (type Consumer.log & popd & exit /b 1)
Consumer.exe
if errorlevel 1 (popd & exit /b 1)
if "%~1"=="atomic-compatible" (
  nsc.exe --cpp-clang-root=A:\al82dev-clang --dump-project-oop-llvm Consumer.nsproj > native.ll 2> native-ir.log
  if errorlevel 1 (type native-ir.log & popd & exit /b 1)
  findstr /r /c:"load atomic i32.*@internal_value.*seq_cst" native.ll >nul
  if errorlevel 1 (popd & exit /b 1)
  findstr /r /c:"atomicrmw add.*@internal_value.*seq_cst" native.ll >nul
  if errorlevel 1 (popd & exit /b 1)
)
popd
echo Global externa compartida validada.
exit /b 0
:expect_incompatible
(
  if not errorlevel 1 (echo Se aceptaron declaraciones incompatibles. & popd & exit /b 1)
  findstr /c:"ambiguous namespace state reference" Consumer.log >nul
  if errorlevel 1 (type Consumer.log & popd & exit /b 1)
  popd
  echo Declaraciones C incompatibles rechazadas.
  exit /b 0
)
