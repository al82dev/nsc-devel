@echo off
setlocal
set "TOOLS=A:\al82dev-clang\bin"
set "DEST=A:\nsc-interop-build\cpp-fixtures\lifetime-emission"
set "CRT=A:\vc2026-runtime\lib\x64\release"
set "UCRT=A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\ucrt\x64"
set "SDK=A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\um\x64"
call "%~dp0ejecutar-local-dll.cmd"
if errorlevel 1 exit /b 1
for %%O in (0 2) do (
  "%TOOLS%\clang.exe" --target=x86_64-pc-windows-msvc -std=c++20 -fexceptions -fcxx-exceptions -O%%O -DCPP_CONTROL -c "%~dp0LocalDoubleException.cpp" -o "%DEST%\double-control-O%%O.obj"
  if errorlevel 1 exit /b 1
  "%TOOLS%\lld-link.exe" /nologo /entry:mainCRTStartup /subsystem:console /nodefaultlib /out:"%DEST%\double-control-O%%O.exe" "%DEST%\double-control-O%%O.obj" "%CRT%\libcmt.lib" "%CRT%\libvcruntime.lib" "%UCRT%\libucrt.lib" "%SDK%\kernel32.lib"
  if errorlevel 1 exit /b 1
  "%DEST%\double-control-O%%O.exe"
  if errorlevel 78 exit /b 1
  if not errorlevel 77 exit /b 1
  "%TOOLS%\clang.exe" --target=x86_64-pc-windows-msvc -std=c++20 -fexceptions -fcxx-exceptions -O%%O -DCPP_DLL_CONTROL -c "%~dp0LocalDoubleException.cpp" -o "%DEST%\double-dll-control-O%%O.obj"
  if errorlevel 1 exit /b 1
  "%TOOLS%\lld-link.exe" /nologo /entry:mainCRTStartup /subsystem:console /nodefaultlib /out:"%DEST%\double-dll-control-O%%O.exe" "%DEST%\double-dll-control-O%%O.obj" "%DEST%\local-throw.lib" "%CRT%\libcmt.lib" "%CRT%\libvcruntime.lib" "%UCRT%\libucrt.lib" "%SDK%\kernel32.lib"
  if errorlevel 1 exit /b 1
  "%DEST%\double-dll-control-O%%O.exe"
  if errorlevel 78 exit /b 1
  if not errorlevel 77 exit /b 1
  "%TOOLS%\clang.exe" --target=x86_64-pc-windows-msvc -std=c++20 -fexceptions -fcxx-exceptions -O%%O -c "%~dp0LocalDoubleException.cpp" -o "%DEST%\double-ns-harness-O%%O.obj"
  if errorlevel 1 exit /b 1
  "%TOOLS%\lld-link.exe" /nologo /entry:mainCRTStartup /subsystem:console /nodefaultlib /out:"%DEST%\double-ns-O%%O.exe" "%DEST%\double-ns-harness-O%%O.obj" "%DEST%\local-throw-ns-O%%O.obj" "%DEST%\local-throw.lib" "A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib" "%CRT%\libcmt.lib" "%CRT%\libvcruntime.lib" "%UCRT%\libucrt.lib" "%SDK%\kernel32.lib" "%SDK%\Shell32.lib"
  if errorlevel 1 exit /b 1
  "%DEST%\double-ns-O%%O.exe"
  if errorlevel 78 exit /b 1
  if not errorlevel 77 exit /b 1
)
echo Doble excepcion: control Clang y NS terminaron mediante el controlador en O0 y O2.
exit /b 0
