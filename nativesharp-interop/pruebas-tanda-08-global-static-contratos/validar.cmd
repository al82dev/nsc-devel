@echo off
setlocal
set "DEST=A:\nsc-interop-build\tanda08-static-contratos-%RANDOM%-%RANDOM%"
if exist "%DEST%" exit /b 1
mkdir "%DEST%"
if errorlevel 1 exit /b 1
set "PATH=A:\al82dev-clang\bin;A:\nsc-interop-build\tanda1-ON\bin;%PATH%"
set "NSC_LINKER=A:\al82dev-clang\bin\lld-link.exe"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
set "TMP=%DEST%"
set "TEMP=%DEST%"
for %%F in (Api.h Control.c Main.ns Project.nsproj ProjectDll.nsproj Library.c Producer.ns Producer.nsproj Consumer.ns Consumer.nsproj Dll.ns Dll.nsproj DllConsumer.ns DllConsumer.nsproj Negative.ns Negative.nsproj NegativeRecord.ns NegativeRecord.nsproj Unsupported.h Unsupported.ns Unsupported.nsproj) do (
  copy /y "%~dp0%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
copy /y "%~dp0..\pruebas-tanda-08-tls-c\static-global\Crt.h" "%DEST%\Crt.h" >nul
if errorlevel 1 exit /b 1
clang.exe --target=x86_64-pc-windows-msvc -std=c11 -c "%DEST%\Control.c" -o "%DEST%\control.obj"
if errorlevel 1 exit /b 1
lld-link.exe /entry:main /subsystem:console /nodefaultlib /out:"%DEST%\control.exe" "%DEST%\control.obj" "A:\vc2026-runtime\lib\x64\release\libcmt.lib" "A:\vc2026-runtime\lib\x64\release\libcpmt.lib" "A:\vc2026-runtime\lib\x64\release\libvcruntime.lib" "A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\ucrt\x64\libucrt.lib" "A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\um\x64\kernel32.lib"
if errorlevel 1 exit /b 1
"%DEST%\control.exe"
if errorlevel 1 exit /b 1
echo Control C de contratos internos correcto.
clang.exe --target=x86_64-pc-windows-msvc -std=c11 -c "%DEST%\Library.c" -o "%DEST%\library.obj"
if errorlevel 1 exit /b 1
lld-link.exe /dll /noentry /nodefaultlib /out:"%DEST%\library.dll" /implib:"%DEST%\library.lib" "%DEST%\library.obj"
if errorlevel 1 exit /b 1
clang.exe --target=x86_64-pc-windows-msvc -std=c11 -S -emit-llvm -O0 "%DEST%\Control.c" -o "%DEST%\control.ll"
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --dump-project-oop-llvm "%DEST%\Project.nsproj" > "%DEST%\native.ll" 2> "%DEST%\native-ir.log"
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --dump-project-oop-llvm "%DEST%\ProjectDll.nsproj" > "%DEST%\native-dll.ll" 2> "%DEST%\native-dll-ir.log"
if errorlevel 1 exit /b 1
python "%~dp0verificar-ir.py" "%DEST%\control.ll" "%DEST%\native.ll" "%DEST%\native-dll.ll"
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --check-project-method-bodies "%DEST%\Negative.nsproj" > "%DEST%\negative.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /c:"error: left side of assignment is not assignable" "%DEST%\negative.log" >nul
if errorlevel 1 exit /b 1
findstr /c:"text[0] = 120" "%DEST%\negative.log" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --check-project-method-bodies "%DEST%\NegativeRecord.nsproj" > "%DEST%\negative-record.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /c:"error: left side of assignment is not assignable" "%DEST%\negative-record.log" >nul
if errorlevel 1 (type "%DEST%\negative-record.log" & exit /b 1)
nsc.exe --cpp-clang-root=A:\al82dev-clang --check-project-method-bodies "%DEST%\Unsupported.nsproj" > "%DEST%\unsupported.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /c:"C/C++ global type is not yet mapped" "%DEST%\unsupported.log" >nul
if errorlevel 1 goto :unsupported_failure
echo Const y tipo no representable rechazados correctamente.
mkdir "%DEST%\bin\Debug"
if errorlevel 1 exit /b 1
for %%O in (0 1 2 3) do (
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O%%O --no-artifact-cache -o "%DEST%\consumer-O%%O.exe" > "%DEST%\consumer-O%%O.log" 2>&1
  if errorlevel 1 goto :failure
  "%DEST%\consumer-O%%O.exe"
  if errorlevel 1 exit /b 1
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\ProjectDll.nsproj" -O%%O --no-artifact-cache -o "%DEST%\consumer-dll-O%%O.exe" > "%DEST%\consumer-dll-O%%O.log" 2>&1
  if errorlevel 1 goto :failure
  "%DEST%\consumer-dll-O%%O.exe"
  if errorlevel 1 exit /b 1
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-lib "%DEST%\Producer.nsproj" -O%%O --no-artifact-cache -o "%DEST%\AtomicContractsProducer.lib" > "%DEST%\producer-O%%O.log" 2>&1
  if errorlevel 1 goto :failure
  ren "%DEST%\Api.h" Api.hidden
  if errorlevel 1 exit /b 1
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Consumer.nsproj" -O%%O --no-artifact-cache -o "%DEST%\precompiled-O%%O.exe" > "%DEST%\precompiled-O%%O.log" 2>&1
  if errorlevel 1 goto :failure
  "%DEST%\precompiled-O%%O.exe"
  if errorlevel 1 exit /b 1
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-dll "%DEST%\Dll.nsproj" -O%%O --no-artifact-cache -o "%DEST%\bin\Debug\AtomicContractsDll.dll" > "%DEST%\nsdll-O%%O.log" 2>&1
  if errorlevel 1 goto :failure
  llvm-readobj.exe --coff-tls-directory "%DEST%\bin\Debug\AtomicContractsDll.dll" > "%DEST%\nsdll-tls-O%%O.log" 2>&1
  if errorlevel 1 exit /b 1
  findstr /L /C:"AddressOfIndex:" "%DEST%\nsdll-tls-O%%O.log" >nul
  if errorlevel 1 goto :failure
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\DllConsumer.nsproj" -O%%O --no-artifact-cache -o "%DEST%\nsdll-consumer-O%%O.exe" > "%DEST%\nsdll-consumer-O%%O.log" 2>&1
  if errorlevel 1 goto :failure
  "%DEST%\nsdll-consumer-O%%O.exe"
  if errorlevel 1 exit /b 1
  ren "%DEST%\Api.hidden" Api.h
  if errorlevel 1 exit /b 1
  echo Contratos de globales internas O%%O correctos.
)
exit /b 0
:failure
type "%DEST%\*O*.log"
exit /b 1
:unsupported_failure
type "%DEST%\unsupported.log"
exit /b 1
