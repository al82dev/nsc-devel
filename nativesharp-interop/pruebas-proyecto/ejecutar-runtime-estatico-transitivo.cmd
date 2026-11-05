@echo off
setlocal
call "%~dp0ejecutar-runtime-transitivo.cmd"
if errorlevel 1 exit /b 1
set "SRC=A:\nsc-interop-build\cpp-fixtures\runtime-transitive"
set "DEST=A:\nsc-interop-build\cpp-fixtures\static-runtime-transitive"
set "TOOLS=A:\al82dev-clang\bin"
if not exist "%DEST%\package" mkdir "%DEST%\package"
if not exist "%DEST%\run" mkdir "%DEST%\run"
if not exist "%DEST%\temp" mkdir "%DEST%\temp"
for %%F in (a.dll a.lib b.dll b.lib) do (
  copy /y "%SRC%\%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 2
  if exist "%DEST%\package\%%F" del /q "%DEST%\package\%%F"
)
if exist "%DEST%\run\a.dll" del /q "%DEST%\run\a.dll"
if exist "%DEST%\run\b.dll" del /q "%DEST%\run\b.dll"
set "TMP=%DEST%\temp"
set "TEMP=%DEST%\temp"
copy /y "%~dp0CDllApi.h" "%DEST%\CDllApi.h" >nul
copy /y "%~dp0CRuntimeTransitiveUnused.h" "%DEST%\Unused.h" >nul
copy /y "%~dp0CRuntimePackagingDll.ns.txt" "%DEST%\Producer.ns" >nul
copy /y "%~dp0CRuntimeStaticTransitiveConsumer.ns.txt" "%DEST%\Consumer.ns" >nul
copy /y "%~dp0CRuntimeStaticTransitiveMissing.nsproj" "%DEST%\Missing.nsproj" >nul
copy /y "%~dp0CRuntimeStaticTransitiveProducer.nsproj" "%DEST%\Producer.nsproj" >nul
copy /y "%~dp0CRuntimeStaticTransitiveConsumer.nsproj" "%DEST%\Consumer.nsproj" >nul
if errorlevel 1 exit /b 3
set "NSC_LINKER=C:\native-sharp\bin\nslink.exe"
set "NSC_LLVM_LIB=%TOOLS%\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-lib "%DEST%\Missing.nsproj" -O0 --no-artifact-cache --cpp-clang-root=A:\al82dev-clang -o "%DEST%\package\NativeProducer.lib" > "%DEST%\missing-transitive.log" 2>&1
if not errorlevel 1 exit /b 4
findstr /l /c:"runtime dependency must be declared for publication: b.dll" "%DEST%\missing-transitive.log" >nul
if errorlevel 1 exit /b 5
"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-lib "%DEST%\Producer.nsproj" -O0 --no-artifact-cache --cpp-clang-root=A:\al82dev-clang -o "%DEST%\package\NativeProducer.lib"
if errorlevel 1 exit /b 6
for %%F in (a.dll a.lib b.dll b.lib) do (
  if not exist "%DEST%\package\%%F" exit /b 7
  move /y "%DEST%\%%F" "%DEST%\%%F.original-test" >nul
  if errorlevel 1 exit /b 8
)
"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-exe "%DEST%\Consumer.nsproj" -O0 --no-artifact-cache -o "%DEST%\run\Consumer.exe"
if errorlevel 1 exit /b 9
if not exist "%DEST%\run\a.dll" exit /b 10
if not exist "%DEST%\run\b.dll" exit /b 11
"%DEST%\run\Consumer.exe"
if not errorlevel 42 exit /b 12
if errorlevel 43 exit /b 12
echo DLL transitiva registrada empaquetada y ejecutada: 42.
exit /b 0
