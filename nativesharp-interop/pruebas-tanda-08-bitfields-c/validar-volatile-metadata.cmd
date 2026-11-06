@echo off
setlocal
set "DEST=A:\nsc-interop-build\tanda08-volatile-bitfields-metadata"
set "PATH=A:\al82dev-clang\bin;A:\nsc-interop-build\tanda1-ON\bin;%PATH%"
if not exist "%DEST%" mkdir "%DEST%"
if errorlevel 1 exit /b 1
set "TMP=%DEST%"
set "TEMP=%DEST%"
set "NSC_LINKER=A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
for %%F in (VolatileApi.h VolatileApi.c VolatileProducer.nsproj VolatileProducer.ns VolatileShared.nsproj VolatileMetadata.nsproj VolatileShared.ns) do (
  copy /y "%~dp0%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 "%DEST%\VolatileApi.c" -o "%DEST%\volatile-api.obj"
if errorlevel 1 exit /b 1
llvm-lib.exe /nologo /out:"%DEST%\volatile-api.lib" "%DEST%\volatile-api.obj"
if errorlevel 1 exit /b 1
for %%O in (0 1 2 3) do (
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-lib "%DEST%\VolatileProducer.nsproj" -O%%O --no-artifact-cache -o "%DEST%\CVolatileBitFieldsProducer.lib"
  if errorlevel 1 exit /b 1
  for %%P in (VolatileShared VolatileMetadata) do (
    if %%O==0 (
      nsc.exe --cpp-clang-root=A:\al82dev-clang --dump-project-oop-llvm "%DEST%\%%P.nsproj" > "%DEST%\%%P.ll"
      if errorlevel 1 exit /b 1
    )
    nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\%%P.nsproj" -O%%O --no-artifact-cache -o "%DEST%\%%P-O%%O.exe"
    if errorlevel 1 exit /b 1
    "%DEST%\%%P-O%%O.exe"
    if errorlevel 1 exit /b 1
    echo Campos de bits volatile %%P O%%O: resultado esperado cero.
  )
)
exit /b 0
