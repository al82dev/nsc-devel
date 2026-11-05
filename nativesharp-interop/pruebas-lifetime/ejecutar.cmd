@echo off
setlocal
if "%1"=="--check-exit" (
  for %%I in (1) do (
    cmd /c exit 7
    if errorlevel 1 exit /b 1
  )
  exit /b 0
)
set "DEST=A:\nsc-interop-build\cpp-fixtures\lifetime-emission"
set "TOOLS=A:\al82dev-clang\bin"
if not exist "%DEST%" mkdir "%DEST%"
if not exist "%DEST%\temp" mkdir "%DEST%\temp"
set "TMP=%DEST%\temp"
set "TEMP=%DEST%\temp"
set "PATH=%TOOLS%;%PATH%"
"%TOOLS%\clang.exe" --target=x86_64-pc-windows-msvc -std=c++20 -O0 -c "%~dp0ExplicitReadyMembers.cpp" -o "%DEST%\explicit-ready-members.obj"
if errorlevel 1 exit /b 1
set "BUNDLE_TEST_ARG=lifetime"
if not "%1"=="--bundle-link" goto run_session_tests
set "BUNDLE_TEST_ARG=bundle-link"
"%TOOLS%\clang.exe" --target=x86_64-pc-windows-msvc -std=c++20 -O0 -c "%~dp0BundleMembers.cpp" -o "%DEST%\bundle-members.obj"
if errorlevel 1 exit /b 1
:run_session_tests
"A:\nsc-interop-build\tanda1-ON\bin\nsc_cpp_session_tests.exe" A:/al82dev-clang/lib/clang/23 A:/nsc-interop-build A:/al82dev-clang/bin/clang.exe %BUNDLE_TEST_ARG%
if errorlevel 1 exit /b 1
"%TOOLS%\llvm-nm.exe" --defined-only "%DEST%\observed-bundle.lib" > "%DEST%\observed-bundle-defined.txt"
if errorlevel 1 exit /b 1
findstr /l /c:"??0Observed@Demo@@" "%DEST%\observed-bundle-defined.txt" >nul
if errorlevel 1 exit /b 1
findstr /l /c:"??1Observed@Demo@@" "%DEST%\observed-bundle-defined.txt" >nul
if errorlevel 1 exit /b 1
"%TOOLS%\llvm-nm.exe" --undefined-only "%DEST%\observed-bundle.lib" > "%DEST%\observed-bundle-undefined.txt"
if errorlevel 1 exit /b 1
findstr /l /c:"??0CountedMember@Demo@@" "%DEST%\observed-bundle-undefined.txt" >nul
if errorlevel 1 exit /b 1
findstr /l /c:"??1CountedMember@Demo@@" "%DEST%\observed-bundle-undefined.txt" >nul
if errorlevel 1 exit /b 1
"%DEST%\explicit-ready-builder.exe"
if errorlevel 1 exit /b 1
if not "%1"=="--bundle-link" goto run_other_tests
"%DEST%\bundle-linked.exe"
if errorlevel 1 exit /b 1
:run_other_tests
"%TOOLS%\llvm-dlltool.exe" -m i386:x86-64 -d "%~dp0..\pruebas-llamadas-escalares\kernel32.def" -l "%DEST%\kernel32.lib"
if errorlevel 1 exit /b 1
"%TOOLS%\llvm-dlltool.exe" -m i386:x86-64 -d "%~dp0kernel32-exceptions.def" -l "%DEST%\kernel32-exceptions.lib"
if errorlevel 1 exit /b 1
"%TOOLS%\llvm-dlltool.exe" -m i386:x86-64 -d "%~dp0ntdll-seh.def" -l "%DEST%\ntdll-seh.lib"
if errorlevel 1 exit /b 1
for %%O in (0 2) do (
  "%TOOLS%\clang.exe" --target=x86_64-pc-windows-msvc -std=c++20 -O%%O -c "%DEST%\integrated-local-harness.cpp" -o "%DEST%\integrated-local-harness-O%%O.obj"
  if errorlevel 1 exit /b 1
  "%TOOLS%\lld-link.exe" /nologo /entry:mainCRTStartup /subsystem:console /nodefaultlib /out:"%DEST%\integrated-local-O%%O.exe" "%DEST%\integrated-local-harness-O%%O.obj" "%DEST%\integrated-local-O%%O.lib" "%DEST%\kernel32.lib" "%DEST%\ntdll-seh.lib"
  if errorlevel 1 exit /b 1
  "%DEST%\integrated-local-O%%O.exe"
  if errorlevel 1 exit /b 1
  "%TOOLS%\clang.exe" --target=x86_64-pc-windows-msvc -std=c++20 -O%%O -c "%DEST%\implicit-local-harness.cpp" -o "%DEST%\implicit-local-harness-O%%O.obj"
  if errorlevel 1 exit /b 1
  "%TOOLS%\lld-link.exe" /nologo /entry:mainCRTStartup /subsystem:console /out:"%DEST%\implicit-local-O%%O.exe" /libpath:A:\vc2026-runtime\lib\x64\release /libpath:A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\ucrt\x64 /libpath:A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\um\x64 "%DEST%\implicit-local-harness-O%%O.obj" "%DEST%\implicit-local-ns-O%%O.obj" "%DEST%\implicit-local-ctor-O%%O.obj" "%DEST%\implicit-local-dtor-O%%O.obj" libcmt.lib libvcruntime.lib libucrt.lib kernel32.lib
  if errorlevel 1 exit /b 1
  "%DEST%\implicit-local-O%%O.exe"
  if errorlevel 1 exit /b 1
  "%TOOLS%\lld-link.exe" /nologo /entry:mainCRTStartup /subsystem:console /out:"%DEST%\implicit-local-bundle-O%%O.exe" /libpath:A:\vc2026-runtime\lib\x64\release /libpath:A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\ucrt\x64 /libpath:A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\um\x64 "%DEST%\implicit-local-harness-O%%O.obj" "%DEST%\implicit-local-bundle-O%%O.lib" libcmt.lib libvcruntime.lib libucrt.lib kernel32.lib
  if errorlevel 1 exit /b 1
  "%DEST%\implicit-local-bundle-O%%O.exe"
  if errorlevel 1 exit /b 1
  "%TOOLS%\clang.exe" --target=x86_64-pc-windows-msvc -std=c++20 -fexceptions -fcxx-exceptions -O%%O -c "%DEST%\local-throw-harness.cpp" -o "%DEST%\local-throw-harness-O%%O.obj"
  if errorlevel 1 exit /b 1
  "%TOOLS%\lld-link.exe" /nologo /entry:mainCRTStartup /subsystem:console /nodefaultlib /out:"%DEST%\local-throw-O%%O.exe" "%DEST%\local-throw-harness-O%%O.obj" "%DEST%\local-throw-ns-O%%O.obj" "A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib" "A:\vc2026-runtime\lib\x64\release\libcmt.lib" "A:\vc2026-runtime\lib\x64\release\libvcruntime.lib" "A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\ucrt\x64\libucrt.lib" "A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\um\x64\kernel32.lib" "A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\um\x64\Shell32.lib"
  if errorlevel 1 exit /b 1
  "%DEST%\local-throw-O%%O.exe"
  if errorlevel 1 exit /b 1
  "%TOOLS%\clang.exe" --target=x86_64-pc-windows-msvc -std=c++20 -O%%O -c "%DEST%\local-ready-harness.cpp" -o "%DEST%\local-ready-harness-O%%O.obj"
  if errorlevel 1 exit /b 1
  "%TOOLS%\lld-link.exe" /nologo /entry:mainCRTStartup /subsystem:console /nodefaultlib /out:"%DEST%\local-ready-O%%O.exe" "%DEST%\local-ready-harness-O%%O.obj" "%DEST%\local-ready-ns-O%%O.obj" "%DEST%\kernel32.lib"
  if errorlevel 1 exit /b 1
  "%DEST%\local-ready-O%%O.exe"
  if errorlevel 1 exit /b 1
  "%TOOLS%\clang.exe" --target=x86_64-pc-windows-msvc -std=c++20 -O%%O -c "%DEST%\explicit-ready-harness.cpp" -o "%DEST%\explicit-ready-harness-O%%O.obj"
  if errorlevel 1 exit /b 1
  "%TOOLS%\lld-link.exe" /nologo /entry:mainCRTStartup /subsystem:console /nodefaultlib /out:"%DEST%\explicit-ready-O%%O.exe" "%DEST%\explicit-ready-harness-O%%O.obj" "%DEST%\explicit-ready-ns-O%%O.obj" "%DEST%\kernel32.lib"
  if errorlevel 1 exit /b 1
  "%DEST%\explicit-ready-O%%O.exe"
  if errorlevel 1 exit /b 1
  "C:\native-sharp\bin\nslink.exe" /entry:mainCRTStartup /subsystem:console /nodefaultlib /machine:x64 /out:"%DEST%\explicit-ready-nslink-O%%O.exe" "%DEST%\explicit-ready-harness-O%%O.obj" "%DEST%\explicit-ready-ns-O%%O.obj" "%DEST%\kernel32.lib"
  if errorlevel 1 exit /b 1
  "%DEST%\explicit-ready-nslink-O%%O.exe"
  if errorlevel 1 exit /b 1
  "%TOOLS%\clang.exe" --target=x86_64-pc-windows-msvc -std=c++20 -O%%O -c "%~dp0Unused.cpp" -o "%DEST%\unused-O%%O.obj"
  if errorlevel 1 exit /b 1
  "%TOOLS%\clang.exe" --target=x86_64-pc-windows-msvc -std=c++20 -O%%O -femit-all-decls -c "%~dp0Unused.cpp" -o "%DEST%\all-O%%O.obj"
  if errorlevel 1 exit /b 1
  "%TOOLS%\clang.exe" --target=x86_64-pc-windows-msvc -std=c++20 -O%%O -c "%~dp0Used.cpp" -o "%DEST%\used-O%%O.obj"
  if errorlevel 1 exit /b 1
  "%TOOLS%\clang.exe" --target=x86_64-pc-windows-msvc -std=c++20 -O%%O -c "%~dp0Run.cpp" -o "%DEST%\run-O%%O.obj"
  if errorlevel 1 exit /b 1
  "%TOOLS%\lld-link.exe" /nologo /entry:mainCRTStartup /subsystem:console /nodefaultlib /out:"%DEST%\run-O%%O.exe" "%DEST%\run-O%%O.obj" "%DEST%\used-O%%O.obj" "%DEST%\kernel32.lib"
  if errorlevel 1 exit /b 1
  "%DEST%\run-O%%O.exe"
  if errorlevel 1 exit /b 1
  "%TOOLS%\clang.exe" --target=x86_64-pc-windows-msvc -O%%O -c "%DEST%\implicit-ctor.ll" -o "%DEST%\implicit-ctor-O%%O.obj"
  if errorlevel 1 exit /b 1
  "%TOOLS%\clang.exe" --target=x86_64-pc-windows-msvc -O%%O -c "%DEST%\implicit-dtor.ll" -o "%DEST%\implicit-dtor-O%%O.obj"
  if errorlevel 1 exit /b 1
  "%TOOLS%\clang.exe" --target=x86_64-pc-windows-msvc -O%%O -c "%DEST%\implicit-ctor.ll" -o "%DEST%\implicit-ctor-duplicate-O%%O.obj"
  if errorlevel 1 exit /b 1
  "%TOOLS%\clang.exe" --target=x86_64-pc-windows-msvc -O%%O -c "%DEST%\implicit-dtor.ll" -o "%DEST%\implicit-dtor-duplicate-O%%O.obj"
  if errorlevel 1 exit /b 1
  "%TOOLS%\clang.exe" --target=x86_64-pc-windows-msvc -std=c++20 -O%%O -c "%~dp0RunGenerated.cpp" -o "%DEST%\run-generated-O%%O.obj"
  if errorlevel 1 exit /b 1
  "%TOOLS%\lld-link.exe" /nologo /entry:mainCRTStartup /subsystem:console /nodefaultlib /out:"%DEST%\run-generated-O%%O.exe" "%DEST%\run-generated-O%%O.obj" "%DEST%\nsc-ctor-O%%O.obj" "%DEST%\nsc-dtor-O%%O.obj" "%DEST%\implicit-ctor-duplicate-O%%O.obj" "%DEST%\implicit-dtor-duplicate-O%%O.obj" "%DEST%\kernel32.lib"
  if errorlevel 1 exit /b 1
  "%DEST%\run-generated-O%%O.exe"
  if errorlevel 1 exit /b 1
  "%TOOLS%\clang.exe" --target=x86_64-pc-windows-msvc -std=c++20 -O%%O -c "%DEST%\observed-harness.cpp" -o "%DEST%\observed-harness-O%%O.obj"
  if errorlevel 1 exit /b 1
  "%TOOLS%\lld-link.exe" /nologo /entry:mainCRTStartup /subsystem:console /out:"%DEST%\observed-O%%O.exe" /libpath:A:\vc2026-runtime\lib\x64\release /libpath:A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\ucrt\x64 /libpath:A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\um\x64 "%DEST%\observed-harness-O%%O.obj" "%DEST%\observed-ns-O%%O.obj" "%DEST%\observed-ctor-O%%O.obj" "%DEST%\observed-dtor-O%%O.obj" libcmt.lib libvcruntime.lib libucrt.lib kernel32.lib
  if errorlevel 1 exit /b 1
  "%DEST%\observed-O%%O.exe"
  if errorlevel 1 exit /b 1
  "%TOOLS%\lld-link.exe" /nologo /entry:mainCRTStartup /subsystem:console /out:"%DEST%\observed-archive-O%%O.exe" /libpath:A:\vc2026-runtime\lib\x64\release /libpath:A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\ucrt\x64 /libpath:A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\um\x64 "%DEST%\observed-harness-O%%O.obj" "%DEST%\observed-bundle.lib" libcmt.lib libvcruntime.lib libucrt.lib kernel32.lib
  if errorlevel 1 exit /b 1
  "%DEST%\observed-archive-O%%O.exe"
  if errorlevel 1 exit /b 1
  "%TOOLS%\clang.exe" --target=x86_64-pc-windows-msvc -std=c++20 -fms-extensions -DTRIGGER_FAILURE -O%%O -c "%DEST%\observed-harness.cpp" -o "%DEST%\observed-throw-harness-O%%O.obj"
  if errorlevel 1 exit /b 1
  "%TOOLS%\lld-link.exe" /nologo /entry:mainCRTStartup /subsystem:console /out:"%DEST%\observed-throw-O%%O.exe" /libpath:A:\vc2026-runtime\lib\x64\release /libpath:A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\ucrt\x64 /libpath:A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\um\x64 "%DEST%\observed-throw-harness-O%%O.obj" "%DEST%\observed-ns-O%%O.obj" "%DEST%\observed-ctor-O%%O.obj" "%DEST%\observed-dtor-O%%O.obj" libcmt.lib libvcruntime.lib libucrt.lib kernel32.lib
  if errorlevel 1 exit /b 1
  "%DEST%\observed-throw-O%%O.exe"
  if errorlevel 1 exit /b 1
  "%TOOLS%\clang.exe" --target=x86_64-pc-windows-msvc -std=c++20 -fexceptions -fcxx-exceptions -DCPP_THROW -O%%O -c "%DEST%\observed-harness.cpp" -o "%DEST%\observed-cxx-throw-harness-O%%O.obj"
  if errorlevel 1 exit /b 1
  "%TOOLS%\lld-link.exe" /nologo /entry:mainCRTStartup /subsystem:console /out:"%DEST%\observed-cxx-throw-O%%O.exe" /libpath:A:\vc2026-runtime\lib\x64\release /libpath:A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\ucrt\x64 /libpath:A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\um\x64 "%DEST%\observed-cxx-throw-harness-O%%O.obj" "%DEST%\observed-ns-O%%O.obj" "%DEST%\observed-ctor-O%%O.obj" "%DEST%\observed-dtor-O%%O.obj" libcmt.lib libvcruntime.lib libucrt.lib kernel32.lib
  if errorlevel 1 exit /b 1
  "%DEST%\observed-cxx-throw-O%%O.exe"
  if errorlevel 1 exit /b 1
  for %%F in (unused all used) do (
    "%TOOLS%\llvm-nm.exe" --defined-only "%DEST%\%%F-O%%O.obj" > "%DEST%\%%F-O%%O.txt"
    if errorlevel 1 exit /b 1
  )
)
echo Objetos y listas de simbolos: %DEST%
exit /b 0
