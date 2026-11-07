@echo off
setlocal
set "DEST=A:\nsc-interop-build\tanda08-atomic-global-extra"
set "PATH=A:\al82dev-clang\bin;A:\nsc-interop-build\tanda1-ON\bin;%PATH%"
set "NSC_LINKER=A:\al82dev-clang\bin\lld-link.exe"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
if not exist "%DEST%" mkdir "%DEST%"
if errorlevel 1 exit /b 1
set "TMP=%DEST%"
set "TEMP=%DEST%"
for %%F in (Extra.h Extra.c Extra.ns Extra.nsproj) do (
  copy /y "%~dp0%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 "%DEST%\Extra.c" -o "%DEST%\extra.obj"
if errorlevel 1 exit /b 1
llvm-lib.exe /nologo /out:"%DEST%\extra.lib" "%DEST%\extra.obj"
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Extra.nsproj" -O0 --no-artifact-cache -o "%DEST%\consumer.exe" > "%DEST%\consumer.log" 2>&1
if errorlevel 1 (type "%DEST%\consumer.log" & exit /b 1)
"%DEST%\consumer.exe"
exit /b %ERRORLEVEL%
