@echo off
setlocal
set "DEST=A:\nsc-interop-build\tanda08-inline-c-thinlto"
set "PATH=A:\al82dev-clang\bin;A:\nsc-interop-build\tanda1-ON\bin;%PATH%"
if not exist "%DEST%" mkdir "%DEST%"
if errorlevel 1 exit /b 1
set "TMP=%DEST%"
set "TEMP=%DEST%"
set "NSC_LINKER=A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
for %%F in (Api.h Api.c Project.nsproj Main.ns Producer.nsproj Producer.ns ProducerSecond.nsproj ProducerSecond.ns Consumer.nsproj Consumer.ns) do (
  copy /y "%~dp0%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 "%DEST%\Api.c" -o "%DEST%\api.obj"
if errorlevel 1 exit /b 1
llvm-lib.exe /nologo /out:"%DEST%\api.lib" "%DEST%\api.obj"
if errorlevel 1 exit /b 1
for %%O in (0 1 2 3) do (
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O%%O -flto=thin --no-artifact-cache -o "%DEST%\inline-O%%O.exe" > "%DEST%\direct-O%%O.log" 2>&1
  if errorlevel 1 exit /b 1
  "%DEST%\inline-O%%O.exe"
  if errorlevel 1 exit /b 1
  if not %%O==0 (
    findstr /l /c:"thinlto-index: " "%DEST%\direct-O%%O.log" >nul
    if errorlevel 1 exit /b 1
    findstr /l /c:"thinlto-index: none" "%DEST%\direct-O%%O.log" >nul
    if not errorlevel 1 exit /b 1
  )
  echo stale> "%DEST%\CInlineProducer.nsthinlto"
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-lib "%DEST%\Producer.nsproj" -O%%O -flto=thin --no-artifact-cache -o "%DEST%\CInlineProducer.lib" > "%DEST%\producer-O%%O.log" 2>&1
  if errorlevel 1 exit /b 1
  if %%O==0 if exist "%DEST%\CInlineProducer.nsthinlto" exit /b 1
  if not %%O==0 if not exist "%DEST%\CInlineProducer.nsthinlto" exit /b 1
  echo stale> "%DEST%\CInlineProducerSecond.nsthinlto"
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-lib "%DEST%\ProducerSecond.nsproj" -O%%O -flto=thin --no-artifact-cache -o "%DEST%\CInlineProducerSecond.lib" > "%DEST%\producer2-O%%O.log" 2>&1
  if errorlevel 1 exit /b 1
  if %%O==0 if exist "%DEST%\CInlineProducerSecond.nsthinlto" exit /b 1
  if not %%O==0 if not exist "%DEST%\CInlineProducerSecond.nsthinlto" exit /b 1
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Consumer.nsproj" -O%%O -flto=thin --no-artifact-cache -o "%DEST%\consumer-O%%O.exe" > "%DEST%\consumer-O%%O.log" 2>&1
  if errorlevel 1 exit /b 1
  "%DEST%\consumer-O%%O.exe"
  if errorlevel 1 exit /b 1
  if not %%O==0 (
    findstr /l /c:"thinlto-libraries: 2" "%DEST%\consumer-O%%O.log" >nul
    if errorlevel 1 exit /b 1
  )
  echo ThinLTO inline C y bibliotecas precompiladas O%%O: correcto.
)
exit /b 0
