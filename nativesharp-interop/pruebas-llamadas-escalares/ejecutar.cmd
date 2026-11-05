@echo off
setlocal
set "OUT=A:\nsc-interop-build\cpp-fixtures\semantic"
set "TOOLS=A:\al82dev-clang\bin"
set "FIXTURES=%~dp0"
"%TOOLS%\llvm-dlltool.exe" -m i386:x86-64 -d "%FIXTURES%kernel32.def" -l "%OUT%\kernel32-prueba.lib"
if errorlevel 1 exit /b 1
"%TOOLS%\clang.exe" --target=x86_64-pc-windows-msvc -c "%FIXTURES%c-externo.cpp" -o "%OUT%\c-externo.obj"
if errorlevel 1 exit /b 1
"%TOOLS%\clang.exe" --target=x86_64-pc-windows-msvc -c "%FIXTURES%variadic-externo.cpp" -o "%OUT%\variadic-externo.obj"
if errorlevel 1 exit /b 1
"%TOOLS%\clang.exe" --target=x86_64-pc-windows-msvc -c "%FIXTURES%cpp-externo.cpp" -o "%OUT%\cpp-externo.obj"
if errorlevel 1 exit /b 1
"%TOOLS%\clang.exe" --target=x86_64-pc-windows-msvc -c "%FIXTURES%pointer-externo.cpp" -o "%OUT%\pointer-externo.obj"
if errorlevel 1 exit /b 1
"%TOOLS%\clang.exe" --target=x86_64-pc-windows-msvc -c "%FIXTURES%cpp-pointer-externo.cpp" -o "%OUT%\cpp-pointer-externo.obj"
if errorlevel 1 exit /b 1
"%TOOLS%\clang.exe" --target=x86_64-pc-windows-msvc -c "%FIXTURES%cpp-record-pointer-externo.cpp" -o "%OUT%\cpp-record-pointer-externo.obj"
if errorlevel 1 exit /b 1
"%TOOLS%\clang.exe" --target=x86_64-pc-windows-msvc -c "%FIXTURES%cpp-record-inferred-externo.cpp" -o "%OUT%\cpp-record-inferred-externo.obj"
if errorlevel 1 exit /b 1
"%TOOLS%\clang.exe" --target=x86_64-pc-windows-msvc -c "%FIXTURES%cpp-record-repeated-externo.cpp" -o "%OUT%\cpp-record-repeated-externo.obj"
if errorlevel 1 exit /b 1
"%TOOLS%\clang.exe" --target=x86_64-pc-windows-msvc -c "%FIXTURES%cpp-instance-externo.cpp" -o "%OUT%\cpp-instance-externo.obj"
if errorlevel 1 exit /b 1
"%TOOLS%\clang.exe" --target=x86_64-pc-windows-msvc -c "%FIXTURES%cpp-dll-field-library.cpp" -o "%OUT%\cpp-dll-field-library.obj"
if errorlevel 1 exit /b 1
"%TOOLS%\lld-link.exe" /nologo /dll /noentry /nodefaultlib /out:"%OUT%\cpp-dll-field-library.dll" /implib:"%OUT%\cpp-dll-field-library.lib" "%OUT%\cpp-dll-field-library.obj"
if errorlevel 1 exit /b 1
"%TOOLS%\clang.exe" --target=x86_64-pc-windows-msvc -c "%FIXTURES%cpp-dll-field-entry.cpp" -o "%OUT%\cpp-dll-field-entry.obj"
if errorlevel 1 exit /b 1
"%TOOLS%\clang.exe" --target=x86_64-pc-windows-msvc -c "%FIXTURES%cpp-global-externo.cpp" -o "%OUT%\cpp-global-externo.obj"
if errorlevel 1 exit /b 1
"%TOOLS%\clang.exe" --target=x86_64-pc-windows-msvc -c "%FIXTURES%const-pointer-externo.cpp" -o "%OUT%\const-pointer-externo.obj"
if errorlevel 1 exit /b 1
"%TOOLS%\clang.exe" --target=x86_64-pc-windows-msvc -std=c++20 -c "%FIXTURES%cpp-const-overload-reference.cpp" -o "%OUT%\cpp-const-overload-reference.obj"
if errorlevel 1 exit /b 1
"%TOOLS%\clang.exe" --target=x86_64-pc-windows-msvc -c "%FIXTURES%cpp-const-overload-externo.cpp" -o "%OUT%\cpp-const-overload-externo.obj"
if errorlevel 1 exit /b 1
"%TOOLS%\lld-link.exe" /nologo /entry:mainCRTStartup /subsystem:console /nodefaultlib /out:"%OUT%\c-call.exe" "%OUT%\c-call.obj" "%OUT%\c-externo.obj" "%OUT%\kernel32-prueba.lib"
if errorlevel 1 exit /b 1
"%TOOLS%\lld-link.exe" /nologo /entry:mainCRTStartup /subsystem:console /nodefaultlib /out:"%OUT%\variadic-call.exe" "%OUT%\variadic-call.obj" "%OUT%\variadic-externo.obj" "%OUT%\kernel32-prueba.lib"
if errorlevel 1 exit /b 1
"%TOOLS%\lld-link.exe" /nologo /entry:mainCRTStartup /subsystem:console /nodefaultlib /out:"%OUT%\cpp-call.exe" "%OUT%\cpp-call.obj" "%OUT%\cpp-externo.obj" "%OUT%\kernel32-prueba.lib"
if errorlevel 1 exit /b 1
"%TOOLS%\lld-link.exe" /nologo /entry:mainCRTStartup /subsystem:console /nodefaultlib /out:"%OUT%\pointer-call.exe" "%OUT%\pointer-call.obj" "%OUT%\pointer-externo.obj" "%OUT%\kernel32-prueba.lib"
if errorlevel 1 exit /b 1
"%TOOLS%\lld-link.exe" /nologo /entry:mainCRTStartup /subsystem:console /nodefaultlib /out:"%OUT%\cpp-pointer-call.exe" "%OUT%\cpp-pointer-call.obj" "%OUT%\cpp-pointer-externo.obj" "%OUT%\kernel32-prueba.lib"
if errorlevel 1 exit /b 1
"%TOOLS%\lld-link.exe" /nologo /entry:mainCRTStartup /subsystem:console /nodefaultlib /out:"%OUT%\cpp-record-pointer-call.exe" "%OUT%\cpp-record-pointer-call.obj" "%OUT%\cpp-record-pointer-externo.obj" "%OUT%\kernel32-prueba.lib"
if errorlevel 1 exit /b 1
"%TOOLS%\lld-link.exe" /nologo /entry:mainCRTStartup /subsystem:console /nodefaultlib /out:"%OUT%\cpp-record-inferred-call.exe" "%OUT%\cpp-record-inferred-call.obj" "%OUT%\cpp-record-inferred-externo.obj" "%OUT%\kernel32-prueba.lib"
if errorlevel 1 exit /b 1
"%TOOLS%\lld-link.exe" /nologo /entry:mainCRTStartup /subsystem:console /nodefaultlib /out:"%OUT%\cpp-record-repeated-call.exe" "%OUT%\cpp-record-repeated-call.obj" "%OUT%\cpp-record-repeated-externo.obj" "%OUT%\kernel32-prueba.lib"
if errorlevel 1 exit /b 1
"%TOOLS%\lld-link.exe" /nologo /entry:mainCRTStartup /subsystem:console /nodefaultlib /out:"%OUT%\cpp-instance-call.exe" "%OUT%\cpp-instance-call.obj" "%OUT%\cpp-instance-externo.obj" "%OUT%\kernel32-prueba.lib"
if errorlevel 1 exit /b 1
"%TOOLS%\lld-link.exe" /nologo /entry:mainCRTStartup /subsystem:console /nodefaultlib /out:"%OUT%\cpp-dll-field-call.exe" "%OUT%\cpp-dll-field-call.obj" "%OUT%\cpp-dll-field-entry.obj" "%OUT%\cpp-dll-field-library.lib" "%OUT%\kernel32-prueba.lib"
if errorlevel 1 exit /b 1
"%TOOLS%\lld-link.exe" /nologo /entry:mainCRTStartup /subsystem:console /nodefaultlib /out:"%OUT%\cpp-global-call.exe" "%OUT%\cpp-global-call.obj" "%OUT%\cpp-global-externo.obj" "%OUT%\kernel32-prueba.lib"
if errorlevel 1 exit /b 1
"%TOOLS%\lld-link.exe" /nologo /entry:mainCRTStartup /subsystem:console /nodefaultlib /out:"%OUT%\const-pointer-call.exe" "%OUT%\const-pointer-call.obj" "%OUT%\const-pointer-externo.obj" "%OUT%\kernel32-prueba.lib"
if errorlevel 1 exit /b 1
"%TOOLS%\lld-link.exe" /nologo /entry:mainCRTStartup /subsystem:console /nodefaultlib /out:"%OUT%\cpp-const-overload-call.exe" "%OUT%\cpp-const-overload-call.obj" "%OUT%\cpp-const-overload-externo.obj" "%OUT%\kernel32-prueba.lib"
if errorlevel 1 exit /b 1
"%OUT%\c-call.exe"
if errorlevel 1 exit /b 1
"%OUT%\variadic-call.exe"
if errorlevel 1 exit /b 1
"%OUT%\cpp-call.exe"
if errorlevel 1 exit /b 1
"%OUT%\pointer-call.exe"
if errorlevel 1 exit /b 1
"%OUT%\cpp-pointer-call.exe"
if errorlevel 1 exit /b 1
"%OUT%\cpp-record-pointer-call.exe"
if errorlevel 1 exit /b 1
"%OUT%\cpp-record-inferred-call.exe"
if errorlevel 1 exit /b 1
"%OUT%\cpp-record-repeated-call.exe"
if errorlevel 1 exit /b 1
"%OUT%\cpp-instance-call.exe"
if errorlevel 1 exit /b 1
"%OUT%\cpp-dll-field-call.exe"
if errorlevel 1 exit /b 1
"%OUT%\cpp-global-call.exe"
if errorlevel 1 exit /b 1
"%OUT%\const-pointer-call.exe"
if errorlevel 1 exit /b 1
"%OUT%\cpp-const-overload-call.exe"
if errorlevel 1 exit /b 1
echo Pruebas de llamadas C y C++ correctas.
exit /b 0
