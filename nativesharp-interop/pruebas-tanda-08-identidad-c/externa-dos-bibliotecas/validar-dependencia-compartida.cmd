@echo off
setlocal
set "DEST=A:\nsc-interop-build\t08-externa-compartida-gate"
if "%~1"=="distintas" set "DEST=A:\nsc-interop-build\t08-externa-distintas-guardado"
set "PATH=A:\nsc-interop-build\tanda1-ON\bin;A:\al82dev-clang\bin;%PATH%"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
set "NSC_LINKER=A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
for %%D in (a b) do (
  if not exist "%DEST%\%%D" mkdir "%DEST%\%%D"
  for %%F in (same.h Producer.ns) do (
    copy /y "%~dp0%%D\%%F" "%DEST%\%%D\%%F" >nul
    if errorlevel 1 exit /b 1
  )
  copy /y "%~dp0%%D\Producer-shared.nsproj" "%DEST%\%%D\Producer.nsproj" >nul
  if errorlevel 1 exit /b 1
  if exist "%DEST%\%%D\fixture.lib" del "%DEST%\%%D\fixture.lib"
  if exist "%DEST%\%%D\fixture.lib" exit /b 1
)
for %%F in (fixture.c Consumer.ns) do (
  copy /y "%~dp0%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
copy /y "%~dp0Consumer-shared.nsproj" "%DEST%\Consumer.nsproj" >nul
if errorlevel 1 exit /b 1
if "%~1"=="distintas" (
  copy /y "%~dp0b\Other.h" "%DEST%\b\same.h" >nul || exit /b 1
  copy /y "%~dp0b\Other.ns" "%DEST%\b\Producer.ns" >nul || exit /b 1
  copy /y "%~dp0b\Producer-other.nsproj" "%DEST%\b\Producer.nsproj" >nul || exit /b 1
  copy /y "%~dp0Consumer-distintas.ns" "%DEST%\Consumer.ns" >nul || exit /b 1
  copy /y "%~dp0fixture-b.c" "%DEST%\fixture-b.c" >nul || exit /b 1
)
pushd "%DEST%" || exit /b 1
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 fixture.c -o fixture.obj
if errorlevel 1 (popd & exit /b 1)
llvm-lib.exe /nologo /out:fixture.lib fixture.obj
if errorlevel 1 (popd & exit /b 1)
if "%~1"=="distintas" (
  clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 fixture-b.c -o b\fixture.obj
  if errorlevel 1 (popd & exit /b 1)
  llvm-lib.exe /nologo /out:b\fixture.lib b\fixture.obj
  if errorlevel 1 (popd & exit /b 1)
)
for %%D in (a b) do (
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-lib "%%D\Producer.nsproj" -O0 --no-artifact-cache -o "%%D\InlineProducer%%D.lib" > "%%D\build.log" 2>&1
  if errorlevel 1 (type "%%D\build.log" & popd & exit /b 1)
)
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe Consumer.nsproj -O0 --no-artifact-cache -o Consumer.exe > Consumer.log 2>&1
if errorlevel 1 (type Consumer.log & popd & exit /b 1)
Consumer.exe
if errorlevel 1 (popd & exit /b 1)
popd
echo Dependencia C compartida entre dos productores validada.
