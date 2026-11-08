@echo off
setlocal
set "DEST=A:\nsc-interop-build\tanda08-inline-thinlto-simbolo"
set "PATH=A:\al82dev-clang\bin;A:\nsc-interop-build\tanda1-ON\bin;%PATH%"
set "NSC_LINKER=A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
if not exist "%DEST%" mkdir "%DEST%"
if errorlevel 1 exit /b 1
set "TEMP=%DEST%"
set "TMP=%DEST%"
for %%F in (Api.h Main.ns Project.nsproj Producer.ns Producer.nsproj Consumer.ns Consumer.nsproj) do (
  copy /y "%~dp0%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
for %%O in (0 1 2 3) do (
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O%%O -flto=thin --no-artifact-cache -o "%DEST%\consumer-O%%O.exe" > "%DEST%\consumer-O%%O.log" 2>&1
  if errorlevel 1 exit /b 1
  "%DEST%\consumer-O%%O.exe"
  if errorlevel 1 exit /b 1
  if not %%O==0 (
    findstr /l /c:"thinlto-index: " "%DEST%\consumer-O%%O.log" >nul
    if errorlevel 1 exit /b 1
    findstr /l /c:"thinlto-index: none" "%DEST%\consumer-O%%O.log" >nul
    if not errorlevel 1 exit /b 1
  )
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-lib "%DEST%\Producer.nsproj" -O%%O -flto=thin --no-artifact-cache -o "%DEST%\InlineSymbolProducer.lib" > "%DEST%\producer-O%%O.log" 2>&1
  if errorlevel 1 exit /b 1
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Consumer.nsproj" -O%%O -flto=thin --no-artifact-cache -o "%DEST%\precompiled-O%%O.exe" > "%DEST%\precompiled-O%%O.log" 2>&1
  if errorlevel 1 exit /b 1
  "%DEST%\precompiled-O%%O.exe"
  if errorlevel 1 exit /b 1
  if not %%O==0 (
    findstr /l /c:"thinlto-libraries: 1" "%DEST%\precompiled-O%%O.log" >nul
    if errorlevel 1 exit /b 1
  )
  echo Simbolo NativeSharp llamado desde inline C O%%O: correcto.
)
exit /b 0
