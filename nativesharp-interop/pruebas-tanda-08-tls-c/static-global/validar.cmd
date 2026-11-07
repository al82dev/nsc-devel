@echo off
setlocal
set "DEST=A:\nsc-interop-build\tanda08-tls-static-global"
set "PATH=A:\al82dev-clang\bin;A:\nsc-interop-build\tanda1-ON\bin;%PATH%"
set "NSC_LINKER=A:\al82dev-clang\bin\lld-link.exe"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
if not exist "%DEST%" mkdir "%DEST%"
if errorlevel 1 exit /b 1
set "TMP=%DEST%"
set "TEMP=%DEST%"
for %%F in (Api.h Api.c Main.ns Probe.c Crt.h Project.nsproj) do (
  copy /y "%~dp0%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 "%DEST%\Api.c" -o "%DEST%\api.obj"
if errorlevel 1 exit /b 1
llvm-lib.exe /nologo /out:"%DEST%\api.lib" "%DEST%\api.obj"
if errorlevel 1 exit /b 1
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 "%DEST%\Probe.c" -o "%DEST%\probe.obj"
if errorlevel 1 exit /b 1
lld-link.exe /nologo /nodefaultlib /subsystem:console /entry:entry /out:"%DEST%\probe.exe" "%DEST%\probe.obj" "%DEST%\api.lib" "A:\vc2026-runtime\lib\x64\release\libcmt.lib" "A:\vc2026-runtime\lib\x64\release\libcpmt.lib" "A:\vc2026-runtime\lib\x64\release\libvcruntime.lib" "A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\ucrt\x64\libucrt.lib" "A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\um\x64\kernel32.lib"
if errorlevel 1 exit /b 1
"%DEST%\probe.exe"
if errorlevel 1 exit /b 1
echo Control C global TLS estatico: correcto.
nsc.exe --cpp-clang-root=A:\al82dev-clang --dump-project-oop-llvm "%DEST%\Project.nsproj" > "%DEST%\project.ll" 2> "%DEST%\ir.log"
if errorlevel 1 exit /b 1
findstr /L /C:"@shared_tls = external thread_local global i32" "%DEST%\project.ll" >nul
if errorlevel 1 exit /b 1
for %%O in (0 1 2 3) do (
  nsc.exe build "%DEST%\Project.nsproj" --cpp-clang-root=A:\al82dev-clang --no-restore --linker LLD -O%%O > "%DEST%\build-O%%O.log" 2>&1
  if errorlevel 1 (echo Compilacion NativeSharp TLS O%%O fallo. & exit /b 1)
  "%DEST%\bin\Debug\CTlsStaticGlobal.exe"
  if errorlevel 1 exit /b 1
  echo Global TLS estatico desde NativeSharp O%%O: correcto.
)
>> "%DEST%\Api.h" echo struct UnsupportedTls { int value; };
>> "%DEST%\Api.h" echo extern __declspec(thread) volatile struct UnsupportedTls unsupported_tls;
> "%DEST%\Main.ns" echo int Main() { return unsupported_tls.value; }
nsc.exe --cpp-clang-root=A:\al82dev-clang --check-project-method-bodies "%DEST%\Project.nsproj" > "%DEST%\unsupported.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /L /C:"thread-local C/C++ global is not yet projected" "%DEST%\unsupported.log" >nul
if errorlevel 1 exit /b 1
echo Global TLS record volatile no representado: rechazado.
exit /b 0
