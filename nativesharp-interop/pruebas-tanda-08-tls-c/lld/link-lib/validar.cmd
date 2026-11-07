@echo off
setlocal EnableDelayedExpansion
set "DEST=A:\nsc-interop-build\tanda08-link-lib\run-%RANDOM%-%RANDOM%"
if exist "%DEST%" exit /b 1
mkdir "%DEST%"
if errorlevel 1 exit /b 1
set "PATH=A:\nsc-interop-build\tanda1-ON\bin;A:\al82dev-clang\bin;A:\nsc-interop-build\cpp-fixtures\command-cpp;%PATH%"
set "NSC_LINKER="
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
set "TMP=%DEST%"
set "TEMP=%DEST%"
for %%F in (Main.ns Exe.nsproj Dll.ns Dll.nsproj Plain.ns Plain.nsproj Consumer.ns Consumer.nsproj) do (
  copy /y "%~dp0%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
for %%F in (Api.c Api.h) do (
  copy /y "%~dp0..\..\%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
copy /y "%~dp0..\NoTls.c" "%DEST%\NoTls.c" >nul
if errorlevel 1 exit /b 1
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 "%DEST%\Api.c" -o "%DEST%\api.obj"
if errorlevel 1 exit /b 1
llvm-lib.exe /nologo /out:"%DEST%\api.lib" "%DEST%\api.obj"
if errorlevel 1 exit /b 1
set "CRTLIBS=--link-lib A:\vc2026-runtime\lib\x64\release\libcmt.lib --link-lib A:\vc2026-runtime\lib\x64\release\libcpmt.lib --link-lib A:\vc2026-runtime\lib\x64\release\libvcruntime.lib --link-lib A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\ucrt\x64\libucrt.lib --link-lib A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\um\x64\kernel32.lib"
for %%O in (0 1 2 3) do (
  nsc.exe --emit-project-oop-exe "%DEST%\Exe.nsproj" --no-artifact-cache -O%%O -o "%DEST%\program-%%O.exe" --link-lib "%DEST%\api.lib" !CRTLIBS! > "%DEST%\exe-%%O.log" 2>&1
  if not "!ERRORLEVEL!"=="0" (type "%DEST%\exe-%%O.log" & exit /b 1)
  "%DEST%\program-%%O.exe"
  if not "!ERRORLEVEL!"=="0" exit /b 1
  nsc.exe --emit-project-oop-dll "%DEST%\Dll.nsproj" --no-artifact-cache -O%%O -o "%DEST%\producer-%%O.dll" --link-lib "%DEST%\api.lib" !CRTLIBS! > "%DEST%\dll-%%O.log" 2>&1
  if not "!ERRORLEVEL!"=="0" (type "%DEST%\dll-%%O.log" & exit /b 1)
  llvm-readobj.exe --coff-tls-directory "%DEST%\producer-%%O.dll" > "%DEST%\pe-%%O.log"
  if not "!ERRORLEVEL!"=="0" exit /b 1
  findstr /L /C:"AddressOfIndex:" "%DEST%\pe-%%O.log" >nul
  if errorlevel 1 exit /b 1
  python "%~dp0validar-dll.py" "%DEST%\producer-%%O.dll"
  if not "!ERRORLEVEL!"=="0" exit /b 1
)
mkdir "%DEST%\consumer"
if errorlevel 1 exit /b 1
nsc.exe --emit-project-oop-exe "%DEST%\Consumer.nsproj" --no-artifact-cache -O0 -o "%DEST%\consumer\consumer.exe" > "%DEST%\consumer.log" 2>&1
if errorlevel 1 (type "%DEST%\consumer.log" & exit /b 1)
"%DEST%\consumer\consumer.exe"
if not "%ERRORLEVEL%"=="0" exit /b 1
nsc.exe --emit-project-oop-exe "%DEST%\Exe.nsproj" --nsc-linker-selection=NSLink --no-artifact-cache -O0 -o "%DEST%\explicit.exe" --link-lib "%DEST%\api.lib" %CRTLIBS% > "%DEST%\explicit.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /L /C:"NSLink failed" "%DEST%\explicit.log" >nul
if errorlevel 1 (type "%DEST%\explicit.log" & exit /b 1)
set "NSC_LINKER=A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
nsc.exe --emit-project-oop-exe "%DEST%\Exe.nsproj" --no-artifact-cache -O0 -o "%DEST%\env.exe" --link-lib "%DEST%\api.lib" %CRTLIBS% > "%DEST%\env.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /L /C:"NSLink failed" "%DEST%\env.log" >nul
if errorlevel 1 (type "%DEST%\env.log" & exit /b 1)
set "NSC_LINKER="
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 "%DEST%\NoTls.c" -o "%DEST%\plain.obj"
if errorlevel 1 exit /b 1
llvm-lib.exe /nologo /out:"%DEST%\plain.lib" "%DEST%\plain.obj"
if errorlevel 1 exit /b 1
nsc.exe --verbose --emit-project-oop-exe "%DEST%\Plain.nsproj" --incremental-link --no-artifact-cache -O0 -o "%DEST%\plain.exe" --link-lib "%DEST%\plain.lib" > "%DEST%\plain-auto.log" 2>&1
if errorlevel 1 (type "%DEST%\plain-auto.log" & exit /b 1)
nsc.exe --verbose --emit-project-oop-exe "%DEST%\Plain.nsproj" --nsc-linker-selection=NSLink --incremental-link --no-artifact-cache -O0 -o "%DEST%\plain.exe" --link-lib "%DEST%\plain.lib" > "%DEST%\plain-explicit.log" 2>&1
if errorlevel 1 (type "%DEST%\plain-explicit.log" & exit /b 1)
findstr /L /C:"incremental-link: hit" "%DEST%\plain-explicit.log" >nul
if errorlevel 1 (type "%DEST%\plain-explicit.log" & exit /b 1)
"%DEST%\plain.exe"
if not "%ERRORLEVEL%"=="0" exit /b 1
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 -flto=thin "%DEST%\NoTls.c" -o "%DEST%\plain.obj"
if errorlevel 1 exit /b 1
llvm-lib.exe /nologo /out:"%DEST%\plain.lib" "%DEST%\plain.obj"
if errorlevel 1 exit /b 1
nsc.exe --emit-project-oop-exe "%DEST%\Plain.nsproj" --no-artifact-cache -O0 -o "%DEST%\plain-bitcode.exe" --link-lib "%DEST%\plain.lib" > "%DEST%\plain-bitcode.log" 2>&1
if errorlevel 1 (type "%DEST%\plain-bitcode.log" & exit /b 1)
"%DEST%\plain-bitcode.exe"
if not "%ERRORLEVEL%"=="0" exit /b 1
echo TLS por --link-lib, DLL, seleccion explicita y control COFF correctos.
exit /b 0
