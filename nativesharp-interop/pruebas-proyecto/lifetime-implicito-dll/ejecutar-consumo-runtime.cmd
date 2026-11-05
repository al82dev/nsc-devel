@echo off
setlocal
set "DEST=A:\nsc-interop-build\cpp-fixtures\static-cpp-runtime-consumer"
set "TOOLS=A:\al82dev-clang\bin"
set "CACHE=%DEST%\cache-%RANDOM%-%RANDOM%"
if not exist "%DEST%\package" mkdir "%DEST%\package"
if not exist "%DEST%\run" mkdir "%DEST%\run"
if not exist "%DEST%\run-off" mkdir "%DEST%\run-off"
if not exist "%DEST%\temp" mkdir "%DEST%\temp"
if exist "%DEST%\package\api.lib" del /q "%DEST%\package\api.lib"
if exist "%DEST%\package\api.dll" del /q "%DEST%\package\api.dll"
if exist "%DEST%\run\api.dll" del /q "%DEST%\run\api.dll"
if exist "%DEST%\run-off\api.dll" del /q "%DEST%\run-off\api.dll"
if exist "%DEST%\api.source-dll" del /q "%DEST%\api.source-dll"
if exist "%DEST%\api.source-dll" exit /b 31
set "TMP=%DEST%\temp"
set "TEMP=%DEST%\temp"
for %%F in (Api.h Api.cpp StaticRuntimeProducer.nsproj StaticRuntimeConsumer.nsproj WrongRuntimeProducer.nsproj) do (
  copy /y "%~dp0%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
copy /y "%~dp0..\lifetime-implicito\StaticProducer.ns.txt" "%DEST%\StaticProducer.ns" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0..\lifetime-implicito\StaticConsumer.ns.txt" "%DEST%\StaticConsumer.ns" >nul
if errorlevel 1 exit /b 1
set "NSC_LINKER=C:\native-sharp\bin\nslink.exe"
set "NSC_LLVM_LIB=%TOOLS%\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
"%TOOLS%\clang.exe" --target=x86_64-pc-windows-msvc -std=c++20 -DBUILDING_API -c "%DEST%\Api.cpp" -o "%DEST%\api.obj"
if errorlevel 1 exit /b 2
"%TOOLS%\lld-link.exe" /nologo /dll /noentry /nodefaultlib /out:"%DEST%\api.dll" /implib:"%DEST%\api.lib" "%DEST%\api.obj"
if errorlevel 1 exit /b 3
python "%~dp0verificar-import-malformado.py" "%DEST%"
if errorlevel 1 exit /b 30
copy /y "%DEST%\api.dll" "%DEST%\wrong.dll" >nul
if errorlevel 1 exit /b 27
"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-lib "%DEST%\WrongRuntimeProducer.nsproj" -O0 --no-artifact-cache --cpp-clang-root=A:\al82dev-clang -o "%DEST%\package\WrongProducer.lib" > "%DEST%\wrong-runtime.log" 2>&1
if not errorlevel 1 exit /b 28
findstr /l /c:"names DLL 'api.dll' but Runtime is 'wrong.dll'" "%DEST%\wrong-runtime.log" >nul
if errorlevel 1 exit /b 29
del /q "%DEST%\wrong.dll"
>"%DEST%\package\api.dll" echo sentinel-producer-conflict
"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-lib "%DEST%\StaticRuntimeProducer.nsproj" -O0 --no-artifact-cache --cpp-clang-root=A:\al82dev-clang -o "%DEST%\package\NativeProducer.lib" > "%DEST%\producer-conflict.log" 2>&1
if not errorlevel 1 exit /b 22
findstr /l sentinel-producer-conflict "%DEST%\package\api.dll" >nul
if errorlevel 1 exit /b 23
del /q "%DEST%\package\api.dll"
"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-lib "%DEST%\StaticRuntimeProducer.nsproj" -O0 --no-artifact-cache --cpp-clang-root=A:\al82dev-clang -o "%DEST%\package\NativeProducer.lib"
if errorlevel 1 exit /b 4
if not exist "%DEST%\package\api.lib" exit /b 5
if not exist "%DEST%\package\api.dll" exit /b 6
move /y "%DEST%\api.lib" "%DEST%\api.source-lib" >nul
if errorlevel 1 exit /b 7
move /y "%DEST%\api.dll" "%DEST%\api.source-dll" >nul
if errorlevel 1 exit /b 8
"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-exe "%DEST%\StaticRuntimeConsumer.nsproj" -O0 --time --artifact-cache=%CACHE% -o "%DEST%\run\Consumer.exe" > "%DEST%\first.log" 2>&1
if errorlevel 1 exit /b 9
if not exist "%DEST%\run\api.dll" exit /b 10
"%DEST%\run\Consumer.exe"
if not errorlevel 73 exit /b 11
if errorlevel 74 exit /b 11
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-OFF\generated\NativeSharp.Runtime.Win32.lib"
"A:\nsc-interop-build\tanda1-OFF\bin\nsc.exe" --emit-project-oop-exe "%DEST%\StaticRuntimeConsumer.nsproj" -O0 --no-artifact-cache -o "%DEST%\run-off\Consumer.exe" > "%DEST%\off.log" 2>&1
if errorlevel 1 exit /b 24
if not exist "%DEST%\run-off\api.dll" exit /b 25
"%DEST%\run-off\Consumer.exe"
if not errorlevel 73 exit /b 26
if errorlevel 74 exit /b 26
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-exe "%DEST%\StaticRuntimeConsumer.nsproj" -O0 --time --artifact-cache=%CACHE% -o "%DEST%\run\Consumer.exe" > "%DEST%\hit.log" 2>&1
if errorlevel 1 exit /b 12
findstr /l /c:"compilation-bundle-cache: hit" "%DEST%\hit.log" >nul
if errorlevel 1 exit /b 13
move /y "%DEST%\package\api.dll" "%DEST%\package\api.saved" >nul
if errorlevel 1 exit /b 14
>"%DEST%\package\api.dll" echo altered-runtime
"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-exe "%DEST%\StaticRuntimeConsumer.nsproj" -O0 --time --artifact-cache=%CACHE% -o "%DEST%\run\Consumer.exe" > "%DEST%\modified.log" 2>&1
if not errorlevel 1 exit /b 15
findstr /l /c:"identity mismatch" "%DEST%\modified.log" >nul
if errorlevel 1 exit /b 16
del /q "%DEST%\package\api.dll"
"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-exe "%DEST%\StaticRuntimeConsumer.nsproj" -O0 --time --artifact-cache=%CACHE% -o "%DEST%\run\Consumer.exe" > "%DEST%\missing.log" 2>&1
if not errorlevel 1 exit /b 17
findstr /l /c:"not an absolute regular file" "%DEST%\missing.log" >nul
if errorlevel 1 exit /b 18
move /y "%DEST%\package\api.saved" "%DEST%\package\api.dll" >nul
if errorlevel 1 exit /b 19
>"%DEST%\run\api.dll" echo sentinel-runtime-conflict
"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-exe "%DEST%\StaticRuntimeConsumer.nsproj" -O0 --no-artifact-cache -o "%DEST%\run\Consumer.exe" > "%DEST%\conflict.log" 2>&1
if not errorlevel 1 exit /b 20
findstr /l sentinel-runtime-conflict "%DEST%\run\api.dll" >nul
if errorlevel 1 exit /b 21
echo Biblioteca NS con DLL C++ empaquetada y consumida: 73; validaciones correctas.
exit /b 0
