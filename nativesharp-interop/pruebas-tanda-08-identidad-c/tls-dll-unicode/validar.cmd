@echo off
setlocal
call "%~dp0..\tls-dll-distintas\validar.cmd" dos || exit /b 1
set "PATH=A:\al82dev-clang\bin;A:\nsc-interop-build\tanda1-ON\bin;%PATH%"
set "NSC_LINKER=A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
python "%~dp0validar.py" %*
if errorlevel 1 exit /b 1
python "%~dp0validar-mayusculas.py"
exit /b %errorlevel%
