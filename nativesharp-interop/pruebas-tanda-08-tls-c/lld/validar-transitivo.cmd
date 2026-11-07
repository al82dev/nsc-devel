@echo off
setlocal
set "DEST=A:\nsc-interop-build\tanda08-tls-transitivo-lld"
set "PATH=A:\al82dev-clang\bin;A:\nsc-interop-build\tanda1-ON\bin;%PATH%"
set "NSC_LINKER=A:\al82dev-clang\bin\lld-link.exe"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
if not exist "%DEST%" mkdir "%DEST%"
if errorlevel 1 exit /b 1
set "TMP=%DEST%"
set "TEMP=%DEST%"
for %%F in (Api.h Api.c) do (
  copy /y "%~dp0..\%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
for %%F in (Crt.h ProbeApi.h Transitive.ns Transitive.nsproj) do (
  copy /y "%~dp0%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 "%DEST%\Api.c" -o "%DEST%\api.obj"
if errorlevel 1 exit /b 1
llvm-lib.exe /nologo /out:"%DEST%\api.lib" "%DEST%\api.obj"
if errorlevel 1 exit /b 1
> "%DEST%\Sub.h" echo enum HeaderSentinel { ImportedOffset = 0 };
nsc.exe build "%DEST%\Transitive.nsproj" --cpp-clang-root=A:\al82dev-clang --no-restore --linker LLD -O0 > "%DEST%\first.log" 2>&1
if errorlevel 1 exit /b 1
"%DEST%\bin\Debug\CTlsTransitive.exe"
if errorlevel 1 exit /b 1
> "%DEST%\Sub.h" echo enum HeaderSentinel { ImportedOffset = 1 };
nsc.exe build "%DEST%\Transitive.nsproj" --cpp-clang-root=A:\al82dev-clang --no-restore --linker LLD -O0 > "%DEST%\second.log" 2>&1
if errorlevel 1 exit /b 1
"%DEST%\bin\Debug\CTlsTransitive.exe"
if errorlevel 2 exit /b 1
if not errorlevel 1 exit /b 1
nsc.exe build "%DEST%\Transitive.nsproj" --cpp-clang-root=A:\al82dev-clang --no-restore --linker LLD --dependency-report > "%DEST%\report.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /L /C:"dependency report for C/C++ references requires transitive header tracking" "%DEST%\report.log" >nul
if errorlevel 1 exit /b 1
echo Header transitivo actualizado y reporte incompleto rechazado: correcto.
exit /b 0
