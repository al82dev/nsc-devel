@echo off
setlocal
set "TOOLS=A:\al82dev-clang\bin"
set "ROOT=A:\nsc-interop-build\cpp-fixtures\lifetime-emission"
set "DEST=%ROOT%\noexcept-boundary"
set "CRT=A:\vc2026-runtime\lib\x64\release"
set "UCRT=A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\ucrt\x64"
set "SDK=A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\um\x64"
call "%~dp0ejecutar-local-dll.cmd"
if errorlevel 1 exit /b 1
if not exist "%DEST%" mkdir "%DEST%"
"%TOOLS%\clang.exe" --target=x86_64-pc-windows-msvc -std=c++20 -fexceptions -fcxx-exceptions -O0 -DCPP_USE_BOUNDARY -c "%~dp0LocalThrowDll.cpp" -o "%DEST%\local-throw.obj"
if errorlevel 1 exit /b 1
"%TOOLS%\clang.exe" --target=x86_64-pc-windows-msvc -std=c++20 -fexceptions -fcxx-exceptions -O0 -c "%~dp0NoexceptBoundaryReference.cpp" -o "%DEST%\boundary.obj"
if errorlevel 1 exit /b 1
"%TOOLS%\lld-link.exe" /nologo /dll /entry:_DllMainCRTStartup /nodefaultlib /out:"%DEST%\local-throw.dll" /implib:"%DEST%\local-throw.lib" "%DEST%\local-throw.obj" "%DEST%\boundary.obj" "%CRT%\libcmt.lib" "%CRT%\libvcruntime.lib" "%UCRT%\libucrt.lib" "%SDK%\kernel32.lib"
if errorlevel 1 exit /b 1
for %%O in (0 2) do (
  "%TOOLS%\clang.exe" --target=x86_64-pc-windows-msvc -std=c++20 -fexceptions -fcxx-exceptions -O%%O -DCPP_DLL_CONTROL -DCPP_SET_DLL_TERMINATE -c "%~dp0LocalDoubleException.cpp" -o "%DEST%\control-O%%O.obj"
  if errorlevel 1 exit /b 1
  "%TOOLS%\lld-link.exe" /nologo /entry:mainCRTStartup /subsystem:console /nodefaultlib /out:"%DEST%\control-O%%O.exe" "%DEST%\control-O%%O.obj" "%DEST%\local-throw.lib" "%CRT%\libcmt.lib" "%CRT%\libvcruntime.lib" "%UCRT%\libucrt.lib" "%SDK%\kernel32.lib"
  if errorlevel 1 exit /b 1
  "%DEST%\control-O%%O.exe"
  if errorlevel 78 exit /b 1
  if not errorlevel 77 exit /b 1
  "%TOOLS%\clang.exe" --target=x86_64-pc-windows-msvc -std=c++20 -fexceptions -fcxx-exceptions -O%%O -DCPP_SET_DLL_TERMINATE -c "%~dp0LocalDoubleException.cpp" -o "%DEST%\ns-harness-O%%O.obj"
  if errorlevel 1 exit /b 1
  "%TOOLS%\lld-link.exe" /nologo /entry:mainCRTStartup /subsystem:console /nodefaultlib /out:"%DEST%\ns-O%%O.exe" "%DEST%\ns-harness-O%%O.obj" "%ROOT%\local-throw-ns-O%%O.obj" "%DEST%\local-throw.lib" "A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib" "%CRT%\libcmt.lib" "%CRT%\libvcruntime.lib" "%UCRT%\libucrt.lib" "%SDK%\kernel32.lib" "%SDK%\Shell32.lib"
  if errorlevel 1 exit /b 1
  "%DEST%\ns-O%%O.exe"
  if errorlevel 78 exit /b 1
  if not errorlevel 77 exit /b 1
)
echo Frontera noexcept: terminacion comprobada en control y NS O0/O2.
exit /b 0
