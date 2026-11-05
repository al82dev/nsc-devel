@echo off
setlocal
set "DEST=A:\nsc-interop-build\cpp-fixtures\runtime-packaging"
set "TOOLS=A:\al82dev-clang\bin"
if not exist "%DEST%\out" mkdir "%DEST%\out"
if exist "%DEST%\out\c-dll.dll" del /q "%DEST%\out\c-dll.dll"
copy /y "%~dp0CDllApi.h" "%DEST%\CDllApi.h" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0CDllMain.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0CRuntimePackaging.nsproj" "%DEST%\Project.nsproj" >nul
if errorlevel 1 exit /b 1
set "NSC_LINKER=C:\native-sharp\bin\nslink.exe"
set "NSC_LLVM_LIB=%TOOLS%\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
"%TOOLS%\clang.exe" --target=x86_64-pc-windows-msvc -std=c11 -c "%~dp0CDllApi.c" -o "%DEST%\c-dll.obj"
if errorlevel 1 exit /b 1
"%TOOLS%\lld-link.exe" /nologo /dll /noentry /nodefaultlib /out:"%DEST%\c-dll.dll" /implib:"%DEST%\c-dll.lib" "%DEST%\c-dll.obj"
if errorlevel 1 exit /b 1
"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-exe "%DEST%\Project.nsproj" --no-artifact-cache --cpp-clang-root=A:\al82dev-clang -o "%DEST%\out\App.exe"
if errorlevel 1 exit /b 1
if not exist "%DEST%\out\c-dll.dll" exit /b 2
"%DEST%\out\App.exe"
if not errorlevel 42 exit /b 1
if errorlevel 43 exit /b 1
echo DLL empaquetada y ejecutable funcionando: 42.
>"%DEST%\out\c-dll.dll" echo sentinel-conflict
"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-exe "%DEST%\Project.nsproj" --no-artifact-cache --cpp-clang-root=A:\al82dev-clang -o "%DEST%\out\App.exe"
if not errorlevel 1 exit /b 1
findstr /l sentinel-conflict "%DEST%\out\c-dll.dll" >nul
if errorlevel 1 exit /b 1
echo Colision detectada y archivo existente conservado.
exit /b 0
