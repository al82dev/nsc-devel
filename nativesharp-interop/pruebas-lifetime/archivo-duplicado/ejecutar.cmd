@echo off
setlocal
set "OUT=A:\nsc-interop-build\cpp-fixtures\archivo-duplicado"
set "TOOLS=A:\al82dev-clang\bin"
if not exist "%OUT%" mkdir "%OUT%"
for %%F in (main first second) do (
  "%TOOLS%\clang.exe" --target=x86_64-pc-windows-msvc -c "%~dp0%%F.c" -o "%OUT%\%%F.obj"
  if errorlevel 1 exit /b 1
)
"%TOOLS%\llvm-lib.exe" /out:"%OUT%\first.lib" "%OUT%\first.obj"
if errorlevel 1 exit /b 1
"%TOOLS%\llvm-lib.exe" /out:"%OUT%\second.lib" "%OUT%\second.obj"
if errorlevel 1 exit /b 1
"%TOOLS%\lld-link.exe" /nodefaultlib /entry:main /subsystem:console /machine:x64 /out:"%OUT%\lld.exe" "%OUT%\main.obj" "%OUT%\first.lib" "%OUT%\second.lib" > "%OUT%\lld.log" 2>&1
if errorlevel 1 exit /b 1
"C:\native-sharp\bin\nslink.exe" /nodefaultlib /entry:main /subsystem:console /machine:x64 /out:"%OUT%\nslink.exe" "%OUT%\main.obj" "%OUT%\first.lib" "%OUT%\second.lib" > "%OUT%\nslink.log" 2>&1
exit /b %errorlevel%
