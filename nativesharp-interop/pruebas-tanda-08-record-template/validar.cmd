@echo off
setlocal
if "%NSC_T08_BUILD%"=="" set "NSC_T08_BUILD=A:\nsc-interop-build\tanda1-ON"
if "%NSC_T08_OUT%"=="" set "NSC_T08_OUT=A:\nsc-interop-build\tanda08-record-template"
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
for %%F in (fixture.h fixture.c Producer.ns Producer.nsproj) do (copy /y "%~dp0%%F" "%NSC_T08_OUT%\producer\%%F" >nul || exit /b 1)
for %%F in (Consumer.ns Consumer.nsproj) do (copy /y "%~dp0%%F" "%NSC_T08_OUT%\consumer\%%F" >nul || exit /b 1)
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 -O%NSC_T08_OPT% "%NSC_T08_OUT%\producer\fixture.c" -o "%NSC_T08_OUT%\producer\fixture.obj"
if errorlevel 1 exit /b 1
llvm-lib.exe /nologo /out:"%NSC_T08_OUT%\producer\fixture.lib" "%NSC_T08_OUT%\producer\fixture.obj"
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=%NSC_T08_CLANG% --emit-project-oop-lib "%NSC_T08_OUT%\producer\Producer.nsproj" -O%NSC_T08_OPT% --no-artifact-cache -o "%NSC_T08_OUT%\producer\RecordProducer.lib"
if errorlevel 1 exit /b 1
for %%F in (RecordProducer.lib RecordProducer.nsmeta fixture.lib) do (copy /y "%NSC_T08_OUT%\producer\%%F" "%NSC_T08_OUT%\consumer\%%F" >nul || exit /b 1)
nsc.exe --cpp-clang-root=%NSC_T08_CLANG% --emit-project-oop-exe "%NSC_T08_OUT%\consumer\Consumer.nsproj" -O%NSC_T08_OPT% --no-artifact-cache -o "%NSC_T08_OUT%\consumer\Consumer.exe"
if errorlevel 1 exit /b 1
"%NSC_T08_OUT%\consumer\Consumer.exe"
exit /b %errorlevel%
