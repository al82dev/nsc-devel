@echo off
setlocal
set "DEST=A:\nsc-interop-build\tanda08-emision-auto\run-%RANDOM%-%RANDOM%"
if exist "%DEST%" exit /b 1
mkdir "%DEST%"
if errorlevel 1 exit /b 1
set "PATH=A:\nsc-interop-build\tanda1-ON\bin;A:\al82dev-clang\bin;A:\nsc-interop-build\cpp-fixtures\command-cpp;%PATH%"
set "NSC_LINKER="
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
set "TMP=%DEST%"
set "TEMP=%DEST%"
for %%F in (Api.c Api.h Main.ns Project.nsproj) do (
  copy /y "%~dp0..\%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
for %%F in (NoTls.c NoTls.h NoTls.ns NoTls.nsproj) do (
  copy /y "%~dp0%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
type nul > "%DEST%\Crt.h"
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 "%DEST%\Api.c" -o "%DEST%\api.obj"
if errorlevel 1 exit /b 1
llvm-lib.exe /nologo /out:"%DEST%\api.lib" "%DEST%\api.obj"
if errorlevel 1 exit /b 1
for %%O in (0 1 2 3) do (
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" --no-artifact-cache -O%%O -o "%DEST%\tls-%%O.exe" > "%DEST%\tls-%%O.log" 2>&1
  if errorlevel 1 (type "%DEST%\tls-%%O.log" & exit /b 1)
  "%DEST%\tls-%%O.exe"
  if errorlevel 1 exit /b 1
)
nsc.exe --verbose --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" --incremental-link --no-artifact-cache -O0 -o "%DEST%\cached.exe" > "%DEST%\auto.log" 2>&1
if errorlevel 1 (type "%DEST%\auto.log" & exit /b 1)
nsc.exe --verbose --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" --nsc-linker-selection=LLD --incremental-link --no-artifact-cache -O0 -o "%DEST%\cached.exe" > "%DEST%\lld.log" 2>&1
if errorlevel 1 (type "%DEST%\lld.log" & exit /b 1)
findstr /L /C:"incremental-link: hit" "%DEST%\lld.log" >nul
if errorlevel 1 (type "%DEST%\lld.log" & exit /b 1)
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" --nsc-linker-selection=NSLink --no-artifact-cache -O0 -o "%DEST%\explicit.exe" > "%DEST%\explicit.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /L /C:"NSLink failed" "%DEST%\explicit.log" >nul
if errorlevel 1 (type "%DEST%\explicit.log" & exit /b 1)
set "NSC_LINKER=A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" --no-artifact-cache -O0 -o "%DEST%\env.exe" > "%DEST%\env.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /L /C:"NSLink failed" "%DEST%\env.log" >nul
if errorlevel 1 (type "%DEST%\env.log" & exit /b 1)
set "NSC_LINKER="
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 "%DEST%\NoTls.c" -o "%DEST%\notls.obj"
if errorlevel 1 exit /b 1
llvm-lib.exe /nologo /out:"%DEST%\notls.lib" "%DEST%\notls.obj"
if errorlevel 1 exit /b 1
nsc.exe --verbose --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\NoTls.nsproj" --incremental-link --no-artifact-cache -O0 -o "%DEST%\notls.exe" > "%DEST%\notls-auto.log" 2>&1
if errorlevel 1 (type "%DEST%\notls-auto.log" & exit /b 1)
nsc.exe --verbose --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\NoTls.nsproj" --nsc-linker-selection=NSLink --incremental-link --no-artifact-cache -O0 -o "%DEST%\notls.exe" > "%DEST%\notls-explicit.log" 2>&1
if errorlevel 1 (type "%DEST%\notls-explicit.log" & exit /b 1)
findstr /L /C:"incremental-link: hit" "%DEST%\notls-explicit.log" >nul
if errorlevel 1 (type "%DEST%\notls-explicit.log" & exit /b 1)
"%DEST%\notls.exe"
if errorlevel 1 exit /b 1
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 -flto=thin "%DEST%\NoTls.c" -o "%DEST%\notls.obj"
if errorlevel 1 exit /b 1
llvm-lib.exe /nologo /out:"%DEST%\notls.lib" "%DEST%\notls.obj"
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\NoTls.nsproj" --no-artifact-cache -O0 -o "%DEST%\bitcode.exe" > "%DEST%\bitcode.log" 2>&1
if errorlevel 1 (type "%DEST%\bitcode.log" & exit /b 1)
"%DEST%\bitcode.exe"
if errorlevel 1 exit /b 1
echo Emision directa TLS, bitcode, selecciones y cache correctos.
exit /b 0
