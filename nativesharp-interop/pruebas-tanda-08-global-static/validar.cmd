@echo off
setlocal
set "DEST=A:\nsc-interop-build\tanda08-global-static-%RANDOM%-%RANDOM%"
if exist "%DEST%" exit /b 1
mkdir "%DEST%"
if errorlevel 1 exit /b 1
set "PATH=A:\al82dev-clang\bin;A:\nsc-interop-build\tanda1-ON\bin;%PATH%"
set "NSC_LINKER=A:\al82dev-clang\bin\lld-link.exe"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
set "TMP=%DEST%"
set "TEMP=%DEST%"
for %%F in (Api.h Control.c Main.ns Project.nsproj) do (
  copy /y "%~dp0%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
clang.exe --target=x86_64-pc-windows-msvc -std=c11 -c "%DEST%\Control.c" -o "%DEST%\control.obj"
if errorlevel 1 exit /b 1
lld-link.exe /entry:main /subsystem:console /nodefaultlib /out:"%DEST%\control.exe" "%DEST%\control.obj"
if errorlevel 1 exit /b 1
"%DEST%\control.exe"
if errorlevel 1 exit /b 1
echo Control C ejecutado correctamente.
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O0 --no-artifact-cache -o "%DEST%\consumer.exe"
if errorlevel 1 exit /b 1
"%DEST%\consumer.exe"
if errorlevel 1 exit /b 1
echo Global estatica y funciones inline comparten estado.
