@echo off
setlocal
set "TOOLS=A:\al82dev-clang\bin"
set "DEST=A:\nsc-interop-build\cpp-fixtures\lifetime-emission"
set "CRT=A:\vc2026-runtime\lib\x64\release"
set "UCRT=A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\ucrt\x64"
set "SDK=A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\um\x64"
call "%~dp0ejecutar-doble-excepcion.cmd"
if errorlevel 1 exit /b 1
for %%O in (0 2) do (
  "C:\native-sharp\bin\nslink.exe" /nologo /entry:mainCRTStartup /subsystem:console /nodefaultlib /machine:x64 /out:"%DEST%\double-nslink-O%%O.exe" "%DEST%\double-ns-harness-O%%O.obj" "%DEST%\local-throw-ns-O%%O.obj" "%DEST%\local-throw.lib" "A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib" "%CRT%\libcmt.lib" "%CRT%\libvcruntime.lib" "%UCRT%\libucrt.lib" "%SDK%\kernel32.lib" "%SDK%\Shell32.lib" > "%DEST%\double-nslink-build-O%%O.log" 2>&1
  if errorlevel 1 (
    type "%DEST%\double-nslink-build-O%%O.log"
    goto link_failure
  )
  "%TOOLS%\llvm-readobj.exe" --coff-imports "%DEST%\double-nslink-O%%O.exe" > "%DEST%\double-nslink-imports-O%%O.txt"
  if errorlevel 1 exit /b 1
  findstr /l /c:"Name: local-throw.dll" "%DEST%\double-nslink-imports-O%%O.txt" >nul
  if errorlevel 1 exit /b 1
  "%DEST%\double-nslink-O%%O.exe"
  if errorlevel 78 exit /b 1
  if not errorlevel 77 exit /b 1
)
echo NSLink: doble excepcion y dependencia DLL verificadas en O0 y O2.
exit /b 0
:link_failure
exit /b 1
