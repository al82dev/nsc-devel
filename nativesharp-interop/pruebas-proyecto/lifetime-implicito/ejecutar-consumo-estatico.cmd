@echo off
setlocal
set "DEST=A:\nsc-interop-build\cpp-fixtures\static-cpp-consumer"
set "TOOLS=A:\al82dev-clang\bin"
set "CACHE=%DEST%\cache-%RANDOM%-%RANDOM%"
if not exist "%DEST%" mkdir "%DEST%"
if not exist "%DEST%\temp" mkdir "%DEST%\temp"
set "TMP=%DEST%\temp"
set "TEMP=%DEST%\temp"
for %%F in (Api.h Api.cpp ApiUpdated.cpp StaticProducer.nsproj StaticConsumer.nsproj) do (
  copy /y "%~dp0%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
copy /y "%~dp0StaticProducer.ns.txt" "%DEST%\StaticProducer.ns" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0StaticConsumer.ns.txt" "%DEST%\StaticConsumer.ns" >nul
if errorlevel 1 exit /b 1
set "NSC_LINKER=C:\native-sharp\bin\nslink.exe"
set "NSC_LLVM_LIB=%TOOLS%\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
"%TOOLS%\clang.exe" --target=x86_64-pc-windows-msvc -std=c++20 -c "%DEST%\Api.cpp" -o "%DEST%\api.obj"
if errorlevel 1 exit /b 1
"%TOOLS%\llvm-lib.exe" /nologo /out:"%DEST%\api.lib" "%DEST%\api.obj"
if errorlevel 1 exit /b 1
"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-lib "%DEST%\StaticProducer.nsproj" -O0 --no-artifact-cache --cpp-clang-root=A:\al82dev-clang -o "%DEST%\NativeProducer.lib"
if errorlevel 1 exit /b 2
"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-exe "%DEST%\StaticConsumer.nsproj" -O0 --no-artifact-cache -o "%DEST%\Consumer.exe"
if errorlevel 1 exit /b 3
"%DEST%\Consumer.exe"
if not errorlevel 73 exit /b 4
if errorlevel 74 exit /b 4
echo Consumidor de biblioteca NS con dependencia C++ estatica: 73.
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-OFF\generated\NativeSharp.Runtime.Win32.lib"
"A:\nsc-interop-build\tanda1-OFF\bin\nsc.exe" --emit-project-oop-exe "%DEST%\StaticConsumer.nsproj" -O0 --no-artifact-cache -o "%DEST%\Consumer-OFF.exe"
if errorlevel 1 exit /b 13
"%DEST%\Consumer-OFF.exe"
if not errorlevel 73 exit /b 14
if errorlevel 74 exit /b 14
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
copy /y "%DEST%\api.lib" "%DEST%\api.saved" >nul
if errorlevel 1 exit /b 5
>"%DEST%\api.lib" echo modified
"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-exe "%DEST%\StaticConsumer.nsproj" -O0 --no-artifact-cache -o "%DEST%\Consumer.exe" > "%DEST%\modified.log" 2>&1
if not errorlevel 1 exit /b 6
findstr /l /c:"identity mismatch" "%DEST%\modified.log" >nul
if errorlevel 1 exit /b 7
del /q "%DEST%\api.lib"
"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-exe "%DEST%\StaticConsumer.nsproj" -O0 --no-artifact-cache -o "%DEST%\Consumer.exe" > "%DEST%\missing.log" 2>&1
if not errorlevel 1 exit /b 8
findstr /l /c:"not an absolute regular file" "%DEST%\missing.log" >nul
if errorlevel 1 exit /b 9
copy /y "%DEST%\api.saved" "%DEST%\api.lib" >nul
if errorlevel 1 exit /b 10
"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-exe "%DEST%\StaticConsumer.nsproj" -O0 --time --artifact-cache=%CACHE% -o "%DEST%\Consumer.exe" > "%DEST%\cache-first.log" 2>&1
if errorlevel 1 exit /b 11
"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-exe "%DEST%\StaticConsumer.nsproj" -O0 --time --artifact-cache=%CACHE% -o "%DEST%\Consumer.exe" > "%DEST%\cache-hit.log" 2>&1
if errorlevel 1 exit /b 12
findstr /l /c:"compilation-bundle-cache: hit" "%DEST%\cache-hit.log" >nul
if errorlevel 1 exit /b 15
"%DEST%\Consumer.exe"
if not errorlevel 73 exit /b 16
if errorlevel 74 exit /b 16
>"%DEST%\api.lib" echo modified-after-cache
"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-exe "%DEST%\StaticConsumer.nsproj" -O0 --time --artifact-cache=%CACHE% -o "%DEST%\Consumer.exe" > "%DEST%\cache-modified.log" 2>&1
if not errorlevel 1 exit /b 17
findstr /l /c:"identity mismatch" "%DEST%\cache-modified.log" >nul
if errorlevel 1 exit /b 18
del /q "%DEST%\api.lib"
"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-exe "%DEST%\StaticConsumer.nsproj" -O0 --time --artifact-cache=%CACHE% -o "%DEST%\Consumer.exe" > "%DEST%\cache-missing.log" 2>&1
if not errorlevel 1 exit /b 19
findstr /l /c:"not an absolute regular file" "%DEST%\cache-missing.log" >nul
if errorlevel 1 exit /b 20
copy /y "%DEST%\api.saved" "%DEST%\api.lib" >nul
if errorlevel 1 exit /b 21
"%TOOLS%\clang.exe" --target=x86_64-pc-windows-msvc -std=c++20 -c "%DEST%\ApiUpdated.cpp" -o "%DEST%\api-updated.obj"
if errorlevel 1 exit /b 22
"%TOOLS%\llvm-lib.exe" /nologo /out:"%DEST%\api.lib" "%DEST%\api-updated.obj"
if errorlevel 1 exit /b 23
"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-lib "%DEST%\StaticProducer.nsproj" -O0 --no-artifact-cache --cpp-clang-root=A:\al82dev-clang -o "%DEST%\NativeProducer.lib" > "%DEST%\updated-producer.log" 2>&1
if errorlevel 1 exit /b 24
"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-exe "%DEST%\StaticConsumer.nsproj" -O0 --time --artifact-cache=%CACHE% -o "%DEST%\Consumer.exe" > "%DEST%\cache-updated.log" 2>&1
if errorlevel 1 exit /b 25
findstr /l /c:"compilation-bundle-cache: hit" "%DEST%\cache-updated.log" >nul
if not errorlevel 1 exit /b 26
"%DEST%\Consumer.exe"
if not errorlevel 74 exit /b 27
if errorlevel 75 exit /b 27
copy /y "%DEST%\api.saved" "%DEST%\api.lib" >nul
if errorlevel 1 exit /b 28
"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-lib "%DEST%\StaticProducer.nsproj" -O0 --no-artifact-cache --cpp-clang-root=A:\al82dev-clang -o "%DEST%\NativeProducer.lib" > "%DEST%\restored-producer.log" 2>&1
if errorlevel 1 exit /b 29
echo Dependencias modificadas o faltantes rechazadas incluso con cache poblada.
exit /b 0
