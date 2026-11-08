@echo off
setlocal
if not defined NSC_T08_BUILD set "NSC_T08_BUILD=A:\nsc-interop-build\tanda1-ON"
if not defined NSC_T08_CLANG set "NSC_T08_CLANG=A:\al82dev-clang"
if not defined NSC_T08_OUT set "NSC_T08_OUT=A:\nsc-interop-build\tanda08-identidad-c"
if not defined NSC_LINKER set "NSC_LINKER=A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
set "DEST=%NSC_T08_OUT%\records-precompiled"
if not exist "%DEST%" mkdir "%DEST%"
if errorlevel 1 exit /b 1
for %%F in (x.h y.h Producer.ns Producer.nsproj Consumer.ns Shared.nsproj Metadata.nsproj) do (
  copy /y "%~dp0%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
set "PATH=%NSC_T08_CLANG%\bin;%NSC_T08_BUILD%\bin;%PATH%"
set "NSC_LLVM_LIB=%NSC_T08_CLANG%\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=%NSC_T08_BUILD%\generated\NativeSharp.Runtime.Win32.lib"
set "TMP=%DEST%"
set "TEMP=%DEST%"
for %%O in (0 1 2 3) do (
  nsc.exe --cpp-clang-root=%NSC_T08_CLANG% --emit-project-oop-lib "%DEST%\Producer.nsproj" -O%%O --no-artifact-cache -o "%DEST%\CCompatibleRecords.lib" > "%DEST%\producer-O%%O.log" 2>&1
  if errorlevel 1 (type "%DEST%\producer-O%%O.log" & exit /b 1)
  for %%P in (Shared Metadata) do (
    nsc.exe --cpp-clang-root=%NSC_T08_CLANG% --emit-project-oop-exe "%DEST%\%%P.nsproj" -O%%O --no-artifact-cache -o "%DEST%\%%P-O%%O.exe" > "%DEST%\%%P-O%%O.log" 2>&1
    if errorlevel 1 (type "%DEST%\%%P-O%%O.log" & exit /b 1)
    "%DEST%\%%P-O%%O.exe"
    if errorlevel 1 exit /b 1
    echo Records compatibles %%P O%%O: lectura y escritura correctas.
  )
)
exit /b 0
