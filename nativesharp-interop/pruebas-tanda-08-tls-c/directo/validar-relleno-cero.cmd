@echo off
setlocal
set "DEST=A:\nsc-interop-build\tls-abi\zerofill"
set "SCALARS=A:\nsc-interop-build\tanda08-tls-directo\scalars"
call "%~dp0validar-escalar-crt.cmd"
if errorlevel 1 exit /b 1
python "%~dp0prepare_zero_fill.py"
if errorlevel 1 exit /b 1
rem T08-D20: the Windows loader does not clear the SizeOfZeroFill bytes of a TLS block, so the controls no longer
rem expect an initial zero there (it depended on the heap: the old "race"). They check the raw-data values and that
rem a value written by NativeSharp into the zero-fill bytes is the one the DLL reads.
rem T08-D20: el cargador de Windows no pone en cero el relleno TLS; los controles ya no esperan un cero inicial alli.
set "PATH=A:\al82dev-clang\bin;A:\nsc-interop-build\tanda1-ON\bin;%PATH%"
set "TMP=%DEST%"
set "TEMP=%DEST%"
set "NSC_LINKER=A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
for %%F in (ScalarApi.h RellenoNative.ns RellenoProject.nsproj) do (
  copy /y "%~dp0%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
copy /y "%SCALARS%\api.lib" "%DEST%\api.lib" >nul
if errorlevel 1 exit /b 1
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 "%~dp0RellenoControl.c" -o "%DEST%\relleno-control.obj" > "%DEST%\relleno-control.log" 2>&1
if errorlevel 1 exit /b 1
lld-link.exe /nologo /nodefaultlib /subsystem:console /entry:mainCRTStartup /out:"%DEST%\relleno-control.exe" "%DEST%\relleno-control.obj" "%DEST%\api.lib" "A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\um\x64\kernel32.lib" >> "%DEST%\relleno-control.log" 2>&1
if errorlevel 1 exit /b 1
"%DEST%\relleno-control.exe"
if errorlevel 1 (echo Control C relleno TLS fallo. & exit /b 1)
echo Control C relleno TLS correcto.
for %%O in (0 1 2 3) do (
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\RellenoProject.nsproj" -O%%O --no-artifact-cache -o "%DEST%\relleno-O%%O.exe" > "%DEST%\relleno-nsc-O%%O.log" 2>&1
  if errorlevel 1 (echo Compilacion NS TLS relleno O%%O fallo. & exit /b 1)
  "%DEST%\relleno-O%%O.exe"
  if errorlevel 1 (echo NS TLS relleno O%%O fallo. & exit /b 1)
  echo NS TLS relleno O%%O correcto.
)
exit /b 0
