@echo off
setlocal
set "DEST=A:\nsc-interop-build\tanda08-tls-runtime-lld"
set "PATH=A:\al82dev-clang\bin;A:\nsc-interop-build\tanda1-ON\bin;%PATH%"
if not exist "%DEST%" mkdir "%DEST%"
if errorlevel 1 exit /b 1
set "TMP=%DEST%"
set "TEMP=%DEST%"
for %%F in (Api.h Api.c Main.ns Project.nsproj) do (
  copy /y "%~dp0..\%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 "%DEST%\Api.c" -o "%DEST%\api.obj"
if errorlevel 1 exit /b 1
llvm-lib.exe /nologo /out:"%DEST%\api.lib" "%DEST%\api.obj"
if errorlevel 1 exit /b 1
nsc.exe --dump-runtime-llvm > "%DEST%\runtime.ll"
if errorlevel 1 exit /b 1
clang.exe -c --target=x86_64-pc-windows-msvc -O2 -ffunction-sections "%DEST%\runtime.ll" -o "%DEST%\runtime.obj"
if errorlevel 1 exit /b 1
clang.exe -c --target=x86_64-pc-windows-msvc "%~dp0RuntimeMain.c" -o "%DEST%\main.obj"
if errorlevel 1 exit /b 1
for %%O in (0 1 2 3) do (
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-obj "%DEST%\Project.nsproj" -O%%O -o "%DEST%\native-O%%O.obj"
  if errorlevel 1 exit /b 1
  lld-link.exe /nologo /nodefaultlib /subsystem:console /entry:__nativesharp_start /include:_tls_used /opt:ref /out:"%DEST%\program-O%%O.exe" "%DEST%\runtime.obj" "%DEST%\main.obj" "%DEST%\native-O%%O.obj" "%DEST%\api.lib" "A:\vc2026-runtime\lib\x64\release\libcmt.lib" "A:\vc2026-runtime\lib\x64\release\libvcruntime.lib" "A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\ucrt\x64\libucrt.lib" "A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\um\x64\kernel32.lib" "A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\um\x64\shell32.lib"
  if errorlevel 1 exit /b 1
  "%DEST%\program-O%%O.exe"
  if errorlevel 1 exit /b 1
  echo TLS C con arranque NativeSharp y LLD O%%O: correcto.
)
exit /b 0
