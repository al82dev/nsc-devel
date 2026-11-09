@echo off
setlocal
set "DEST=A:\nsc-interop-build\cpp-fixtures\command-cpp"
set "TOOLS=A:\al82dev-clang\bin"
if not exist "%DEST%" mkdir "%DEST%"
copy /y "%~dp0cpp-api.cpp" "%DEST%\cpp-api.cpp" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0cpp-api.h" "%DEST%\cpp-api.h" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0CppMain.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0CppComando.nsproj" "%DEST%\CppProject.nsproj" >nul
if errorlevel 1 exit /b 1
copy /y "C:\native-sharp\bin\nslink.exe" "%DEST%\nslink.exe" >nul
if errorlevel 1 exit /b 1
set "NSC_LINKER=%DEST%\nslink.exe"
set "NSC_LLVM_LIB=%TOOLS%\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
"%TOOLS%\clang.exe" --target=x86_64-pc-windows-msvc -std=c++20 -c "%DEST%\cpp-api.cpp" -o "%DEST%\cpp-api.obj"
if errorlevel 1 exit /b 1
"%TOOLS%\llvm-lib.exe" /nologo /out:"%DEST%\cpp-api.lib" "%DEST%\cpp-api.obj"
if errorlevel 1 exit /b 1
"%TOOLS%\llvm-nm.exe" "%DEST%\cpp-api.lib" | findstr /l /c:"?blend@tools@@YAHH@Z" >nul
if errorlevel 1 exit /b 1
"%TOOLS%\llvm-nm.exe" "%DEST%\cpp-api.lib" | findstr /l /c:"?blend@tools@@YANN@Z" >nul
if errorlevel 1 exit /b 1
"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-exe "%DEST%\CppProject.nsproj" --no-artifact-cache --cpp-clang-root=A:\al82dev-clang -o "%DEST%\CppProject.exe"
if errorlevel 1 exit /b 1
"%DEST%\CppProject.exe"
if not errorlevel 148 exit /b 1
if errorlevel 149 exit /b 1
copy /y "%~dp0CppConst.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-exe "%DEST%\CppProject.nsproj" --no-artifact-cache --cpp-clang-root=A:\al82dev-clang -o "%DEST%\unsupported.exe" > "%DEST%\const-check.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /c:"left side of assignment is not assignable" "%DEST%\const-check.log" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0CppTls.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-exe "%DEST%\CppProject.nsproj" --no-artifact-cache --cpp-clang-root=A:\al82dev-clang -o "%DEST%\tls.exe" > "%DEST%\tls-check.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /c:"thread-local C/C++ global is not yet projected" "%DEST%\tls-check.log" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0CppAddrConst.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-exe "%DEST%\CppProject.nsproj" --no-artifact-cache --cpp-clang-root=A:\al82dev-clang -o "%DEST%\addrconst.exe" > "%DEST%\addrconst-check.log" 2>&1
if not errorlevel 1 exit /b 1
rem T08-D06: `&` of a const C global is a pointer to const data; storing it in an `int*` drops const.
findstr /c:"would discard pointee constness" "%DEST%\addrconst-check.log" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0CppRefVolatile.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-exe "%DEST%\CppProject.nsproj" --no-artifact-cache --cpp-clang-root=A:\al82dev-clang -o "%DEST%\refvolatile.exe" > "%DEST%\refvolatile-check.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /c:"reference-local initializer must be a mutable addressable storage location" "%DEST%\refvolatile-check.log" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0CppAddrVolatile.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-exe "%DEST%\CppProject.nsproj" --no-artifact-cache --cpp-clang-root=A:\al82dev-clang -o "%DEST%\addrvolatile.exe" > "%DEST%\addrvolatile-check.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /c:"cannot take the address of a qualified C/C++ global" "%DEST%\addrvolatile-check.log" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0CppPointeeConst.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-exe "%DEST%\CppProject.nsproj" --no-artifact-cache --cpp-clang-root=A:\al82dev-clang -o "%DEST%\pointee-const.exe" > "%DEST%\pointee-const-check.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /c:"left side of assignment is not assignable" "%DEST%\pointee-const-check.log" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0CppPointeeLoss.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-exe "%DEST%\CppProject.nsproj" --no-artifact-cache --cpp-clang-root=A:\al82dev-clang -o "%DEST%\pointee-loss.exe" > "%DEST%\pointee-loss-check.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /c:"would discard pointee constness" "%DEST%\pointee-loss-check.log" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0CppPointeeAddress.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-exe "%DEST%\CppProject.nsproj" --no-artifact-cache --cpp-clang-root=A:\al82dev-clang -o "%DEST%\pointee-address.exe" > "%DEST%\pointee-address-check.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /c:"cannot take the address of a qualified C/C++ global" "%DEST%\pointee-address-check.log" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0CppPointeeCallMutable.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-exe "%DEST%\CppProject.nsproj" --no-artifact-cache --cpp-clang-root=A:\al82dev-clang -o "%DEST%\pointee-call.exe" > "%DEST%\pointee-call-check.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /c:"no viable overload" "%DEST%\pointee-call-check.log" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0CppFixedPointerWrite.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-exe "%DEST%\CppProject.nsproj" --no-artifact-cache --cpp-clang-root=A:\al82dev-clang -o "%DEST%\fixed-pointer.exe" > "%DEST%\fixed-pointer-check.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /c:"left side of assignment is not assignable" "%DEST%\fixed-pointer-check.log" >nul
if errorlevel 1 exit /b 1
echo Proyecto C++ y NativeSharp ejecutado: resultado 148.
exit /b 0
