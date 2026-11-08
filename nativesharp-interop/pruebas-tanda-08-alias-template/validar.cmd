@echo off
setlocal
if "%NSC_T08_BUILD%"=="" set "NSC_T08_BUILD=A:\nsc-interop-build\tanda1-ON"
if "%NSC_T08_OUT%"=="" set "NSC_T08_OUT=A:\nsc-interop-build\tanda08-alias-template"
if "%NSC_T08_CLANG%"=="" set "NSC_T08_CLANG=A:\al82dev-clang"
if "%NSC_T08_OPT%"=="" set "NSC_T08_OPT=0"
if "%NSC_LINKER%"=="" set "NSC_LINKER=A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
set "PATH=%NSC_T08_CLANG%\bin;%NSC_T08_BUILD%\bin;%PATH%"
set "NSC_LLVM_LIB=%NSC_T08_CLANG%\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=%NSC_T08_BUILD%\generated\NativeSharp.Runtime.Win32.lib"
if not exist "%NSC_T08_OUT%\producer" mkdir "%NSC_T08_OUT%\producer"
if not exist "%NSC_T08_OUT%\consumer" mkdir "%NSC_T08_OUT%\consumer"
if not exist "%NSC_T08_OUT%\temp" mkdir "%NSC_T08_OUT%\temp"
set "TEMP=%NSC_T08_OUT%\temp"
set "TMP=%TEMP%"
copy /y "%~dp0alias.h" "%NSC_T08_OUT%\producer\alias.h" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0Producer.ns" "%NSC_T08_OUT%\producer\Producer.ns" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0Producer.nsproj" "%NSC_T08_OUT%\producer\Producer.nsproj" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0Consumer.ns" "%NSC_T08_OUT%\consumer\Consumer.ns" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0Consumer.nsproj" "%NSC_T08_OUT%\consumer\Consumer.nsproj" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=%NSC_T08_CLANG% --emit-project-oop-lib "%NSC_T08_OUT%\producer\Producer.nsproj" -O%NSC_T08_OPT% --no-artifact-cache -o "%NSC_T08_OUT%\producer\AliasProducer.lib"
if errorlevel 1 exit /b 1
nsc_cpp_c_identity_tests.exe --inspect-template-alias "%NSC_T08_OUT%\producer\AliasProducer.nsmeta" AliasInt
if errorlevel 1 exit /b 1
copy /y "%NSC_T08_OUT%\producer\AliasProducer.lib" "%NSC_T08_OUT%\consumer\AliasProducer.lib" >nul
if errorlevel 1 exit /b 1
copy /y "%NSC_T08_OUT%\producer\AliasProducer.nsmeta" "%NSC_T08_OUT%\consumer\AliasProducer.nsmeta" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=%NSC_T08_CLANG% --emit-project-oop-exe "%NSC_T08_OUT%\consumer\Consumer.nsproj" -O%NSC_T08_OPT% --no-artifact-cache -o "%NSC_T08_OUT%\consumer\Consumer.exe"
if errorlevel 1 exit /b 1
"%NSC_T08_OUT%\consumer\Consumer.exe"
exit /b %errorlevel%
