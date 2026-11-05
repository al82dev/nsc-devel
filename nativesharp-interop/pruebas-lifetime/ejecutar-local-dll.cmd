@echo off
setlocal
set "TOOLS=A:\al82dev-clang\bin"
set "DEST=A:\nsc-interop-build\cpp-fixtures\lifetime-emission"
set "CRT=A:\vc2026-runtime\lib\x64\release"
set "UCRT=A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\ucrt\x64"
set "SDK=A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\um\x64"
call "%~dp0ejecutar.cmd"
if errorlevel 1 exit /b 1
findstr /l /c:"__nsc_function_Semantic$3A$3A__Main" "%DEST%\local-throw-harness.cpp" >nul
if errorlevel 1 exit /b 1
"%TOOLS%\clang.exe" --target=x86_64-pc-windows-msvc -std=c++20 -fexceptions -fcxx-exceptions -O0 -c "%~dp0LocalThrowDll.cpp" -o "%DEST%\local-throw-dll.obj"
if errorlevel 1 exit /b 1
"%TOOLS%\lld-link.exe" /nologo /dll /entry:_DllMainCRTStartup /nodefaultlib /out:"%DEST%\local-throw.dll" /implib:"%DEST%\local-throw.lib" "%DEST%\local-throw-dll.obj" "%CRT%\libcmt.lib" "%CRT%\libvcruntime.lib" "%UCRT%\libucrt.lib" "%SDK%\kernel32.lib"
if errorlevel 1 exit /b 1
for %%O in (0 2) do (
  "%TOOLS%\clang.exe" --target=x86_64-pc-windows-msvc -std=c++20 -fexceptions -fcxx-exceptions -O%%O -c "%~dp0LocalThrowDllHarness.cpp" -o "%DEST%\local-throw-dll-harness-O%%O.obj"
  if errorlevel 1 exit /b 1
  "%TOOLS%\lld-link.exe" /nologo /entry:mainCRTStartup /subsystem:console /nodefaultlib /out:"%DEST%\local-throw-dll-O%%O.exe" "%DEST%\local-throw-dll-harness-O%%O.obj" "%DEST%\local-throw-ns-O%%O.obj" "%DEST%\local-throw.lib" "A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib" "%CRT%\libcmt.lib" "%CRT%\libvcruntime.lib" "%UCRT%\libucrt.lib" "%SDK%\kernel32.lib" "%SDK%\Shell32.lib"
  if errorlevel 1 exit /b 1
  "%TOOLS%\llvm-readobj.exe" --coff-imports "%DEST%\local-throw-dll-O%%O.exe" > "%DEST%\local-throw-dll-imports-O%%O.txt"
  if errorlevel 1 exit /b 1
  findstr /l /c:"Name: local-throw.dll" "%DEST%\local-throw-dll-imports-O%%O.txt" >nul
  if errorlevel 1 exit /b 1
  findstr /l /c:"??0Risky@Demo@@" "%DEST%\local-throw-dll-imports-O%%O.txt" >nul
  if errorlevel 1 exit /b 1
  findstr /l /c:"??1Risky@Demo@@" "%DEST%\local-throw-dll-imports-O%%O.txt" >nul
  if errorlevel 1 exit /b 1
  findstr /l /c:"cpp_local_throw" "%DEST%\local-throw-dll-imports-O%%O.txt" >nul
  if errorlevel 1 exit /b 1
  "%DEST%\local-throw-dll-O%%O.exe"
  if errorlevel 1 exit /b 1
)
echo Locales C++ en DLL: cinco escenarios correctos en O0 y O2.
exit /b 0
