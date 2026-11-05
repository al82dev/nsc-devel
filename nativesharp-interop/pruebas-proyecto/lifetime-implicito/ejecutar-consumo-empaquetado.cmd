@echo off
setlocal
call "%~dp0ejecutar-consumo-estatico.cmd"
if errorlevel 1 exit /b 1
set "DEST=A:\nsc-interop-build\cpp-fixtures\static-cpp-consumer"
set "TOOLS=A:\al82dev-clang\bin"
if not exist "%DEST%\out" mkdir "%DEST%\out"
copy /y "%~dp0StaticConsumerPackaged.nsproj" "%DEST%\StaticConsumerPackaged.nsproj" >nul
if errorlevel 1 exit /b 1
set "NSC_LINKER=C:\native-sharp\bin\nslink.exe"
set "NSC_LLVM_LIB=%TOOLS%\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
>"%DEST%\out\api.lib" echo sentinel-conflict
"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-lib "%DEST%\StaticProducer.nsproj" -O0 --no-artifact-cache --cpp-clang-root=A:\al82dev-clang -o "%DEST%\out\NativeProducer.lib" > "%DEST%\package-conflict.log" 2>&1
if not errorlevel 1 exit /b 2
findstr /l sentinel-conflict "%DEST%\out\api.lib" >nul
if errorlevel 1 exit /b 3
del /q "%DEST%\out\api.lib"
"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-lib "%DEST%\StaticProducer.nsproj" -O0 --no-artifact-cache --cpp-clang-root=A:\al82dev-clang -o "%DEST%\out\NativeProducer.lib"
if errorlevel 1 exit /b 4
if not exist "%DEST%\out\api.lib" exit /b 5
move /y "%DEST%\api.lib" "%DEST%\api.original-test" >nul
if errorlevel 1 exit /b 11
"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-exe "%DEST%\StaticConsumerPackaged.nsproj" -O0 --no-artifact-cache -o "%DEST%\out\Consumer.exe"
if errorlevel 1 exit /b 6
"%DEST%\out\Consumer.exe"
if not errorlevel 73 exit /b 7
if errorlevel 74 exit /b 7
move /y "%DEST%\api.original-test" "%DEST%\api.lib" >nul
if errorlevel 1 exit /b 12
"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-lib "%DEST%\StaticProducer.nsproj" -O0 --no-artifact-cache --cpp-clang-root=A:\al82dev-clang -o "%DEST%\out\api.lib" > "%DEST%\product-conflict.log" 2>&1
if not errorlevel 1 exit /b 8
findstr /l /c:"conflicts with a NativeSharp product" "%DEST%\product-conflict.log" >nul
if errorlevel 1 exit /b 9
fc /b "%DEST%\api.lib" "%DEST%\out\api.lib" >nul
if errorlevel 1 exit /b 10
echo Biblioteca NS empaquetada con dependencia C++ y consumidor funcionando: 73.
exit /b 0
