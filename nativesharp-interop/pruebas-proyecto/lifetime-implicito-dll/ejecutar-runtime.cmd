@echo off
setlocal
set "DEST=A:\nsc-interop-build\cpp-fixtures\runtime-packaging-cpp"
set "TOOLS=A:\al82dev-clang\bin"
if not exist "%DEST%\out" mkdir "%DEST%\out"
if not exist "%DEST%\temp" mkdir "%DEST%\temp"
if exist "%DEST%\out\api.dll" del /q "%DEST%\out\api.dll"
set "TMP=%DEST%\temp"
set "TEMP=%DEST%\temp"
copy /y "%~dp0Api.h" "%DEST%\Api.h" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0Api.cpp" "%DEST%\Api.cpp" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0Project-runtime.nsproj" "%DEST%\Project.nsproj" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0Main.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
set "NSC_LINKER=C:\native-sharp\bin\nslink.exe"
set "NSC_LLVM_LIB=%TOOLS%\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
"%TOOLS%\clang.exe" --target=x86_64-pc-windows-msvc -std=c++20 -DBUILDING_API -c "%DEST%\Api.cpp" -o "%DEST%\api.obj"
if errorlevel 1 exit /b 1
"%TOOLS%\lld-link.exe" /nologo /dll /noentry /nodefaultlib /out:"%DEST%\api.dll" /implib:"%DEST%\api.lib" "%DEST%\api.obj"
if errorlevel 1 exit /b 1
"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-exe "%DEST%\Project.nsproj" -O0 --no-artifact-cache --cpp-clang-root=A:\al82dev-clang -o "%DEST%\out\App.exe"
if errorlevel 1 exit /b 1
if not exist "%DEST%\out\api.dll" exit /b 2
"%DEST%\out\App.exe"
if not errorlevel 73 exit /b 1
if errorlevel 74 exit /b 1
echo DLL C++ empaquetada y ejecutable funcionando: 73.
exit /b 0
