@echo off
setlocal EnableDelayedExpansion
set "DEST=A:\nsc-interop-build\tanda08-tls-runtime-real-%RANDOM%-%RANDOM%"
if exist "%DEST%" exit /b 1
mkdir "%DEST%"
if errorlevel 1 exit /b 1
set "PATH=A:\al82dev-clang\bin;A:\nsc-interop-build\tanda1-ON\bin;%PATH%"
set "TMP=%DEST%"
set "TEMP=%DEST%"
set "NSC_LINKER=A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
for %%F in (Api.h Api.c Main.ns Project.nsproj) do (
  copy /y "%~dp0..\%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 "%DEST%\Api.c" -o "%DEST%\api.obj"
if errorlevel 1 exit /b 1
llvm-lib.exe /nologo /out:"%DEST%\api.lib" "%DEST%\api.obj"
if errorlevel 1 exit /b 1
for %%O in (0 1 2 3) do (
  set "NSC_KEEP_LINK_TEMPS=%DEST%\capture-O%%O"
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O%%O --no-artifact-cache -o "%DEST%\failed-O%%O.exe" > "%DEST%\nslink-O%%O.log" 2>&1
  if not errorlevel 1 exit /b 1
  findstr /L /C:"_tls_index" "%DEST%\nslink-O%%O.log" >nul
  if errorlevel 1 exit /b 1
  set "TASK_RUNTIME="
  for /d %%D in ("%DEST%\capture-O%%O\nativesharp-stage20-*") do if exist "%%~fD\runtime.obj" set "TASK_RUNTIME=%%~fD\runtime.obj"
  if not defined TASK_RUNTIME exit /b 1
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-obj "%DEST%\Project.nsproj" -O%%O -o "%DEST%\native-O%%O.obj"
  if errorlevel 1 exit /b 1
  lld-link.exe /nologo /nodefaultlib /subsystem:console /entry:__nativesharp_start /opt:ref /out:"%DEST%\program-O%%O.exe" "!TASK_RUNTIME!" "%DEST%\native-O%%O.obj" "%DEST%\api.lib" "%NSC_WINDOWS_RUNTIME_LIBRARY%" "A:\vc2026-runtime\lib\x64\release\libcmt.lib" "A:\vc2026-runtime\lib\x64\release\libvcruntime.lib" "A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\ucrt\x64\libucrt.lib" "A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\um\x64\kernel32.lib" "A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\um\x64\shell32.lib"
  if errorlevel 1 exit /b 1
  "%DEST%\program-O%%O.exe"
  if errorlevel 1 exit /b 1
  echo TLS C con runtime real NSC y LLD O%%O: correcto.
)
echo Evidencia: %DEST%
exit /b 0
