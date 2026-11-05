@echo off
setlocal
call "%~dp0ejecutar-runtime-transitivo.cmd"
if errorlevel 1 exit /b 1
set "SRC=A:\nsc-interop-build\cpp-fixtures\runtime-transitive"
set "DEST=A:\nsc-interop-build\cpp-fixtures\multi-runtime-package"
set "TOOLS=A:\al82dev-clang\bin"
set "CACHE=%DEST%\cache-%RANDOM%-%RANDOM%"
if not exist "%DEST%\package" mkdir "%DEST%\package"
if not exist "%DEST%\run" mkdir "%DEST%\run"
if not exist "%DEST%\run-off" mkdir "%DEST%\run-off"
if not exist "%DEST%\temp" mkdir "%DEST%\temp"
for %%F in (a.dll b.dll both.lib) do (
  if exist "%DEST%\package\%%F" del /q "%DEST%\package\%%F"
  if exist "%DEST%\run\%%F" del /q "%DEST%\run\%%F"
  if exist "%DEST%\run-off\%%F" del /q "%DEST%\run-off\%%F"
  if exist "%DEST%\%%F.original-test" del /q "%DEST%\%%F.original-test"
)
for %%F in (a.dll a.lib b.dll b.lib) do (
  copy /y "%SRC%\%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 17
)
copy /y "%~dp0CDllApi.h" "%DEST%\CDllApi.h" >nul
if errorlevel 1 exit /b 18
copy /y "%~dp0CRuntimeTransitiveUnused.h" "%DEST%\Unused.h" >nul
if errorlevel 1 exit /b 18
copy /y "%~dp0CRuntimeTransitiveUnused.h" "%DEST%\Unused2.h" >nul
if errorlevel 1 exit /b 18
copy /y "%~dp0CRuntimePackagingDll.ns.txt" "%DEST%\Producer.ns" >nul
if errorlevel 1 exit /b 18
copy /y "%~dp0CRuntimeStaticTransitiveConsumer.ns.txt" "%DEST%\Consumer.ns" >nul
if errorlevel 1 exit /b 18
copy /y "%~dp0CRuntimeMultiMissing.nsproj" "%DEST%\Missing.nsproj" >nul
if errorlevel 1 exit /b 18
copy /y "%~dp0CRuntimeMultiExtra.nsproj" "%DEST%\Extra.nsproj" >nul
if errorlevel 1 exit /b 18
copy /y "%~dp0CRuntimeMultiProducer.nsproj" "%DEST%\Producer.nsproj" >nul
if errorlevel 1 exit /b 18
copy /y "%~dp0CRuntimeStaticTransitiveConsumer.nsproj" "%DEST%\Consumer.nsproj" >nul
if errorlevel 1 exit /b 18
"%TOOLS%\llvm-lib.exe" /out:"%DEST%\both.lib" "%DEST%\a.lib" "%DEST%\b.lib"
if errorlevel 1 exit /b 2
set "TMP=%DEST%\temp"
set "TEMP=%DEST%\temp"
set "NSC_LINKER=C:\native-sharp\bin\nslink.exe"
set "NSC_LLVM_LIB=%TOOLS%\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-lib "%DEST%\Missing.nsproj" -O0 --no-artifact-cache --cpp-clang-root=A:\al82dev-clang -o "%DEST%\package\NativeProducer.lib" > "%DEST%\missing.log" 2>&1
if not errorlevel 1 exit /b 3
findstr /l /c:"names DLL 'b.dll' but Runtime is 'a.dll'" "%DEST%\missing.log" >nul
if errorlevel 1 exit /b 4
copy /y "%DEST%\a.dll" "%DEST%\wrong.dll" >nul
if errorlevel 1 exit /b 14
"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-lib "%DEST%\Extra.nsproj" -O0 --no-artifact-cache --cpp-clang-root=A:\al82dev-clang -o "%DEST%\package\Extra.lib" > "%DEST%\extra.log" 2>&1
if not errorlevel 1 exit /b 15
findstr /l /c:"does not name declared Runtime 'wrong.dll'" "%DEST%\extra.log" >nul
if errorlevel 1 exit /b 16
del /q "%DEST%\wrong.dll"
"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-lib "%DEST%\Producer.nsproj" -O0 --no-artifact-cache --cpp-clang-root=A:\al82dev-clang -o "%DEST%\package\NativeProducer.lib"
if errorlevel 1 exit /b 5
for %%F in (a.dll b.dll both.lib) do (
  if not exist "%DEST%\package\%%F" exit /b 6
  move /y "%DEST%\%%F" "%DEST%\%%F.original-test" >nul
  if errorlevel 1 exit /b 7
)
"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-exe "%DEST%\Consumer.nsproj" -O0 --time --artifact-cache=%CACHE% -o "%DEST%\run\Consumer.exe" > "%DEST%\first.log" 2>&1
if errorlevel 1 exit /b 8
for %%F in (a.dll b.dll) do if not exist "%DEST%\run\%%F" exit /b 9
"%DEST%\run\Consumer.exe"
if not errorlevel 42 exit /b 10
if errorlevel 43 exit /b 10
"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-exe "%DEST%\Consumer.nsproj" -O0 --time --artifact-cache=%CACHE% -o "%DEST%\run\Consumer.exe" > "%DEST%\hit.log" 2>&1
if errorlevel 1 exit /b 19
findstr /l /c:"compilation-bundle-cache: hit" "%DEST%\hit.log" >nul
if errorlevel 1 exit /b 20
move /y "%DEST%\package\b.dll" "%DEST%\package\b.saved" >nul
if errorlevel 1 exit /b 21
>"%DEST%\package\b.dll" echo modified-dll
"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-exe "%DEST%\Consumer.nsproj" -O0 --time --artifact-cache=%CACHE% -o "%DEST%\run\Consumer.exe" > "%DEST%\modified.log" 2>&1
if not errorlevel 1 exit /b 22
findstr /l /c:"identity mismatch" "%DEST%\modified.log" >nul
if errorlevel 1 exit /b 23
del /q "%DEST%\package\b.dll"
"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-exe "%DEST%\Consumer.nsproj" -O0 --time --artifact-cache=%CACHE% -o "%DEST%\run\Consumer.exe" > "%DEST%\missing-dll.log" 2>&1
if not errorlevel 1 exit /b 24
findstr /l /c:"not an absolute regular file" "%DEST%\missing-dll.log" >nul
if errorlevel 1 exit /b 25
move /y "%DEST%\package\b.saved" "%DEST%\package\b.dll" >nul
if errorlevel 1 exit /b 26
move /y "%DEST%\package\both.lib" "%DEST%\package\both.saved" >nul
if errorlevel 1 exit /b 27
>"%DEST%\package\both.lib" echo modified-library
"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-exe "%DEST%\Consumer.nsproj" -O0 --time --artifact-cache=%CACHE% -o "%DEST%\run\Consumer.exe" > "%DEST%\modified-lib.log" 2>&1
if not errorlevel 1 exit /b 28
findstr /l /c:"identity mismatch" "%DEST%\modified-lib.log" >nul
if errorlevel 1 exit /b 29
del /q "%DEST%\package\both.lib"
move /y "%DEST%\package\both.saved" "%DEST%\package\both.lib" >nul
if errorlevel 1 exit /b 30
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-OFF\generated\NativeSharp.Runtime.Win32.lib"
"A:\nsc-interop-build\tanda1-OFF\bin\nsc.exe" --emit-project-oop-exe "%DEST%\Consumer.nsproj" -O0 --no-artifact-cache -o "%DEST%\run-off\Consumer.exe"
if errorlevel 1 exit /b 11
for %%F in (a.dll b.dll) do if not exist "%DEST%\run-off\%%F" exit /b 12
"%DEST%\run-off\Consumer.exe"
if not errorlevel 42 exit /b 13
if errorlevel 43 exit /b 13
copy /y "%DEST%\a.dll.original-test" "%DEST%\a.dll" >nul
if errorlevel 1 exit /b 31
copy /y "%DEST%\both.lib.original-test" "%DEST%\both.lib" >nul
if errorlevel 1 exit /b 32
"%TOOLS%\clang.exe" --target=x86_64-pc-windows-msvc -std=c11 -c "%~dp0CRuntimeTransitiveBActualizada.c" -o "%DEST%\B-actualizada.obj"
if errorlevel 1 exit /b 33
"%TOOLS%\lld-link.exe" /nologo /dll /noentry /nodefaultlib /out:"%DEST%\b.dll" /implib:"%DEST%\b-actualizada.lib" "%DEST%\B-actualizada.obj"
if errorlevel 1 exit /b 34
"%TOOLS%\llvm-lib.exe" /out:"%DEST%\both-actualizada.lib" "%DEST%\a.lib" "%DEST%\b-actualizada.lib"
if errorlevel 1 exit /b 47
fc /b "%DEST%\both.lib" "%DEST%\both-actualizada.lib" >nul
if errorlevel 2 exit /b 48
if not errorlevel 1 exit /b 49
del /q "%DEST%\both.lib"
if exist "%DEST%\both.lib" exit /b 50
move /y "%DEST%\both-actualizada.lib" "%DEST%\both.lib" >nul
if errorlevel 1 exit /b 51
del /q "%DEST%\package\b.dll"
if exist "%DEST%\package\b.dll" exit /b 35
del /q "%DEST%\package\both.lib"
if exist "%DEST%\package\both.lib" exit /b 52
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-lib "%DEST%\Producer.nsproj" -O0 --no-artifact-cache --cpp-clang-root=A:\al82dev-clang -o "%DEST%\package\NativeProducer.lib" > "%DEST%\updated-producer.log" 2>&1
if errorlevel 1 exit /b 36
"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-exe "%DEST%\Consumer.nsproj" -O0 --time --artifact-cache=%CACHE% -o "%DEST%\run\Consumer.exe" > "%DEST%\updated-conflict.log" 2>&1
if not errorlevel 1 exit /b 37
findstr /l /c:"runtime package destination already contains a different 'b.dll'" "%DEST%\updated-conflict.log" >nul
if errorlevel 1 exit /b 38
fc /b "%DEST%\run\b.dll" "%DEST%\b.dll.original-test" >nul
if errorlevel 1 exit /b 39
del /q "%DEST%\run\b.dll"
if exist "%DEST%\run\b.dll" exit /b 40
"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-exe "%DEST%\Consumer.nsproj" -O0 --time --artifact-cache=%CACHE% -o "%DEST%\run\Consumer.exe" > "%DEST%\updated.log" 2>&1
if errorlevel 1 exit /b 41
findstr /l /c:"compilation-bundle-cache:" "%DEST%\updated.log" >nul
if errorlevel 1 exit /b 42
findstr /l /c:"compilation-bundle-cache: hit" "%DEST%\updated.log" >nul
if not errorlevel 1 exit /b 43
"%DEST%\run\Consumer.exe"
if not errorlevel 47 exit /b 44
if errorlevel 48 exit /b 44
"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-exe "%DEST%\Consumer.nsproj" -O0 --time --artifact-cache=%CACHE% -o "%DEST%\run\Consumer.exe" > "%DEST%\updated-hit.log" 2>&1
if errorlevel 1 exit /b 45
findstr /l /c:"compilation-bundle-cache: hit" "%DEST%\updated-hit.log" >nul
if errorlevel 1 exit /b 46
echo DLL y biblioteca multidll actualizadas: resultado 42 a 47; cache renovada y conflicto preservado.
exit /b 0
