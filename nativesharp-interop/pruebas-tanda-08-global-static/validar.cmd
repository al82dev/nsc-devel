@echo off
setlocal
set "DEST=A:\nsc-interop-build\tanda08-global-static-%RANDOM%-%RANDOM%"
if exist "%DEST%" exit /b 1
mkdir "%DEST%"
if errorlevel 1 goto :failure
set "PATH=A:\al82dev-clang\bin;A:\nsc-interop-build\tanda1-ON\bin;%PATH%"
set "NSC_LINKER=A:\al82dev-clang\bin\lld-link.exe"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
set "TMP=%DEST%"
set "TEMP=%DEST%"
for %%F in (Api.h Other.h Control.c Main.ns Project.nsproj Solo.ns Solo.nsproj Producer.ns Producer.nsproj ProducerSecond.ns ProducerSecond.nsproj Consumer.ns Consumer.nsproj Ambiguous.ns Ambiguous.nsproj) do (
  copy /y "%~dp0%%F" "%DEST%\%%F" >nul
  if errorlevel 1 goto :failure
)
clang.exe --target=x86_64-pc-windows-msvc -std=c11 -c "%DEST%\Control.c" -o "%DEST%\control.obj"
if errorlevel 1 goto :failure
lld-link.exe /entry:main /subsystem:console /nodefaultlib /out:"%DEST%\control.exe" "%DEST%\control.obj"
if errorlevel 1 goto :failure
"%DEST%\control.exe"
if errorlevel 1 goto :failure
echo Control C ejecutado correctamente.
for %%O in (0 1 2 3) do (
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O%%O --no-artifact-cache -o "%DEST%\consumer-O%%O.exe" > "%DEST%\consumer-O%%O.log" 2>&1
  if errorlevel 1 goto :failure
  "%DEST%\consumer-O%%O.exe"
  if errorlevel 1 goto :failure
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Solo.nsproj" -O%%O --no-artifact-cache -o "%DEST%\solo-O%%O.exe" > "%DEST%\solo-O%%O.log" 2>&1
  if errorlevel 1 goto :failure
  "%DEST%\solo-O%%O.exe"
  if errorlevel 1 goto :failure
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-lib "%DEST%\Producer.nsproj" -O%%O --no-artifact-cache -o "%DEST%\StaticProducer.lib" > "%DEST%\producer-O%%O.log" 2>&1
  if errorlevel 1 goto :failure
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-lib "%DEST%\ProducerSecond.nsproj" -O%%O --no-artifact-cache -o "%DEST%\StaticProducerSecond.lib" > "%DEST%\producer-second-O%%O.log" 2>&1
  if errorlevel 1 goto :failure
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Consumer.nsproj" -O%%O --no-artifact-cache -o "%DEST%\precompiled-O%%O.exe" > "%DEST%\precompiled-O%%O.log" 2>&1
  if errorlevel 1 goto :failure
  "%DEST%\precompiled-O%%O.exe"
  if errorlevel 1 goto :failure
  echo Global estatica con y sin funciones inline O%%O correcta.
)
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Ambiguous.nsproj" -O0 --no-artifact-cache -o "%DEST%\ambiguous.exe" > "%DEST%\ambiguous.log" 2>&1
if not errorlevel 1 goto :failure
findstr /c:"ambiguous C internal global origins" "%DEST%\ambiguous.log" >nul
if errorlevel 1 goto :failure
echo Globales internas ambiguas rechazadas correctamente.
exit /b 0
:failure
type "%DEST%\*.log"
exit /b 1
