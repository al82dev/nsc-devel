@echo off
setlocal
set "DEST=A:\nsc-interop-build\t08-identity-anon-nested-gate"
set "PATH=A:\nsc-interop-build\tanda1-ON\bin;A:\al82dev-clang\bin;%PATH%"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
set "NSC_LINKER=A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
if not exist "%DEST%" mkdir "%DEST%"
if errorlevel 1 exit /b 1
for %%F in (Api.h Api.c Main.ns Main.nsproj Producer.ns Producer.nsproj Consumer.ns Consumer.nsproj) do (
  copy /y "%~dp0%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
pushd "%DEST%"
if errorlevel 1 exit /b 1
c-index-test.exe -test-load-source-usrs all -std=c11 Api.h > usr.log 2>&1
if errorlevel 1 exit /b 1
findstr /l /c:"c:@S@AnonOuter@Sa@Ua@Sa@FI@mode" usr.log >nul
if errorlevel 1 exit /b 1
findstr /l /c:"c:@S@AnonOuter@Sa@Ua@Sa@FI@value" usr.log >nul
if errorlevel 1 exit /b 1
findstr /l /c:"@AnonFirst" usr.log >nul
if errorlevel 1 exit /b 1
findstr /l /c:"@AnonSecond" usr.log >nul
if errorlevel 1 exit /b 1
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 -O0 Api.c -o api.obj
if errorlevel 1 exit /b 1
llvm-lib.exe /nologo /out:api.lib api.obj
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe Main.nsproj -O0 --no-artifact-cache -o Main.exe > Main.log 2>&1
if errorlevel 1 (type Main.log & exit /b 1)
Main.exe
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-lib Producer.nsproj -O0 --no-artifact-cache -o AnonNestedProducer.lib > Producer.log 2>&1
if errorlevel 1 (type Producer.log & exit /b 1)
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe Consumer.nsproj -O0 --no-artifact-cache -o Consumer.exe > Consumer.log 2>&1
if errorlevel 1 (type Consumer.log & exit /b 1)
Consumer.exe
if errorlevel 1 exit /b 1
popd
echo Anonimos anidados: USR de Clang y ejecucion directa y precompilada correctos.
exit /b 0
