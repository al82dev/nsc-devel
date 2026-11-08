@echo off
setlocal
if not defined NSC_T08_BUILD set "NSC_T08_BUILD=A:\nsc-interop-build\tanda1-ON"
if not defined NSC_T08_OUT set "NSC_T08_OUT=A:\nsc-interop-build\tanda08-anonimos-c"
if not defined NSC_T08_NSLINK set "NSC_T08_NSLINK=A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
set "PATH=A:\al82dev-clang\bin;%NSC_T08_BUILD%\bin;%PATH%"
if not exist "%NSC_T08_OUT%" mkdir "%NSC_T08_OUT%"
if errorlevel 1 exit /b 1
set "TMP=%NSC_T08_OUT%"
set "TEMP=%NSC_T08_OUT%"
set "NSC_LINKER=%NSC_T08_NSLINK%"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=%NSC_T08_BUILD%\generated\NativeSharp.Runtime.Win32.lib"
for %%F in (Api.h Api.c Main.ns Producer.ns Consumer.ns Project.nsproj Producer.nsproj Shared.nsproj Metadata.nsproj) do (
  copy /y "%~dp0%%F" "%NSC_T08_OUT%\%%F" >nul
  if errorlevel 1 exit /b 1
)
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 "%NSC_T08_OUT%\Api.c" -o "%NSC_T08_OUT%\api.obj"
if errorlevel 1 exit /b 1
llvm-lib.exe /nologo /out:"%NSC_T08_OUT%\api.lib" "%NSC_T08_OUT%\api.obj"
if errorlevel 1 exit /b 1
for %%O in (0 1 2 3) do (
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%NSC_T08_OUT%\Project.nsproj" -O%%O --no-artifact-cache -o "%NSC_T08_OUT%\Direct-O%%O.exe"
  if errorlevel 1 exit /b 1
  "%NSC_T08_OUT%\Direct-O%%O.exe"
  if errorlevel 1 exit /b 1
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-lib "%NSC_T08_OUT%\Producer.nsproj" -O%%O --no-artifact-cache -o "%NSC_T08_OUT%\CAnonymousProducer.lib"
  if errorlevel 1 exit /b 1
  for %%P in (Shared Metadata) do (
    nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%NSC_T08_OUT%\%%P.nsproj" -O%%O --no-artifact-cache -o "%NSC_T08_OUT%\%%P-O%%O.exe"
    if errorlevel 1 exit /b 1
    "%NSC_T08_OUT%\%%P-O%%O.exe"
    if errorlevel 1 exit /b 1
  )
  echo Miembros anonimos C O%%O: directo, Shared y Metadata pasaron.
)
exit /b 0
