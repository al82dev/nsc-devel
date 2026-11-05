@echo off
setlocal
set "DEST=A:\nsc-interop-build\cpp-fixtures\lifetime-emission"
set "TOOLS=A:\al82dev-clang\bin"
if not exist "%DEST%\observed-bundle.lib" exit /b 1
if not exist "%DEST%\temp" mkdir "%DEST%\temp"
set "TMP=%DEST%\temp"
set "TEMP=%DEST%\temp"
"%TOOLS%\clang.exe" --target=x86_64-pc-windows-msvc -std=c++20 -O0 -c "%~dp0Consumer.cpp" -o "%DEST%\dependency-consumer.obj"
if errorlevel 1 exit /b 1
"%TOOLS%\clang.exe" --target=x86_64-pc-windows-msvc -std=c++20 -O0 -c "%~dp0..\BundleMembers.cpp" -o "%DEST%\dependency-members.obj"
if errorlevel 1 exit /b 1
"%TOOLS%\llvm-lib.exe" /nologo /out:"%DEST%\dependency-members.lib" "%DEST%\dependency-members.obj"
if errorlevel 1 exit /b 1
"%TOOLS%\lld-link.exe" /nologo /entry:mainCRTStartup /subsystem:console /out:"%DEST%\dependency-missing.exe" /libpath:A:\vc2026-runtime\lib\x64\release /libpath:A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\ucrt\x64 /libpath:A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\um\x64 "%DEST%\dependency-consumer.obj" "%DEST%\observed-bundle.lib" libcmt.lib libvcruntime.lib libucrt.lib kernel32.lib > "%DEST%\dependency-missing.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /l /c:"Demo::CountedMember::CountedMember" "%DEST%\dependency-missing.log" >nul
if errorlevel 1 exit /b 1
"%TOOLS%\lld-link.exe" /nologo /entry:mainCRTStartup /subsystem:console /out:"%DEST%\dependency-complete.exe" /libpath:A:\vc2026-runtime\lib\x64\release /libpath:A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\ucrt\x64 /libpath:A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\um\x64 "%DEST%\dependency-consumer.obj" "%DEST%\observed-bundle.lib" "%DEST%\dependency-members.lib" libcmt.lib libvcruntime.lib libucrt.lib kernel32.lib
if errorlevel 1 exit /b 1
"%DEST%\dependency-complete.exe"
if errorlevel 1 exit /b 1
echo Dependencia C++ separada: ausencia rechazada, enlace y ejecucion correctos.
exit /b 0
