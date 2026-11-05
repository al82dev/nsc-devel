@echo off
setlocal
set "DEST=A:\nsc-interop-build\cpp-fixtures\runtime-packaging-dll"
set "TOOLS=A:\al82dev-clang\bin"
if not exist "%DEST%\out" mkdir "%DEST%\out"
if exist "%DEST%\out\c-dll.dll" del /q "%DEST%\out\c-dll.dll"
copy /y "%~dp0CDllApi.h" "%DEST%\CDllApi.h" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0CRuntimePackagingDll.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0CRuntimePackagingDll.nsproj" "%DEST%\Project.nsproj" >nul
if errorlevel 1 exit /b 1
set "NSC_LINKER=C:\native-sharp\bin\nslink.exe"
set "NSC_LLVM_LIB=%TOOLS%\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
"%TOOLS%\clang.exe" --target=x86_64-pc-windows-msvc -std=c11 -c "%~dp0CDllApi.c" -o "%DEST%\c-dll.obj"
if errorlevel 1 exit /b 1
"%TOOLS%\lld-link.exe" /nologo /dll /noentry /nodefaultlib /out:"%DEST%\c-dll.dll" /implib:"%DEST%\c-dll.lib" "%DEST%\c-dll.obj"
if errorlevel 1 exit /b 1
"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-dll "%DEST%\Project.nsproj" --no-artifact-cache --cpp-clang-root=A:\al82dev-clang -o "%DEST%\out\Bridge.dll"
if errorlevel 1 exit /b 1
if not exist "%DEST%\out\c-dll.dll" exit /b 2
"%TOOLS%\llvm-readobj.exe" --coff-imports "%DEST%\out\Bridge.dll" > "%DEST%\imports.txt"
if errorlevel 1 exit /b 1
findstr /l c-dll.dll "%DEST%\imports.txt" >nul
if errorlevel 1 exit /b 1
"%TOOLS%\clang.exe" --target=x86_64-pc-windows-msvc -c "%~dp0CRuntimePackagingHost.c" -o "%DEST%\host.obj"
if errorlevel 1 exit /b 1
"%TOOLS%\lld-link.exe" /nologo /subsystem:console /entry:mainCRTStartup /nodefaultlib /out:"%DEST%\out\Host.exe" "%DEST%\host.obj" "%DEST%\out\Bridge.lib" "A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\um\x64\kernel32.lib"
if errorlevel 1 exit /b 1
"%DEST%\out\Host.exe"
if not errorlevel 42 exit /b 1
if errorlevel 43 exit /b 1
echo DLL NativeSharp importa y empaqueta la DLL C declarada.
exit /b 0
