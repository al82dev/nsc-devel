@echo off
setlocal
set "DEST=A:\nsc-interop-build\cpp-fixtures\published-snapshot"
cmake --build A:\nsc-interop-build\tanda1-ON --target nsc nsc_cpp_session_tests
if errorlevel 1 exit /b 1
if not exist "%DEST%" mkdir "%DEST%"
copy /y "%~dp0Producer.nsproj" "%DEST%\Producer.nsproj" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0Producer.ns" "%DEST%\Producer.ns" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0Consumer.nsproj" "%DEST%\Consumer.nsproj" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0Consumer.ns" "%DEST%\Consumer.ns" >nul
if errorlevel 1 exit /b 1
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_LINKER=A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-lib "%DEST%\Producer.nsproj" -O0 --no-artifact-cache --time -o "%DEST%\SnapshotProducer.lib"
if errorlevel 1 exit /b 1
if not exist "%DEST%\SnapshotProducer.nsproducts" exit /b 1
"A:\nsc-interop-build\tanda1-ON\bin\nsc_cpp_session_tests.exe" A:/al82dev-clang/lib/clang/23 A:/nsc-interop-build A:/al82dev-clang/bin/clang.exe "%DEST%\Consumer.nsproj"
if errorlevel 1 exit /b 1
echo Snapshot publicado cargado y observado correctamente.
exit /b 0
