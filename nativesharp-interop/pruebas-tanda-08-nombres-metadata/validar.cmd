@echo off
setlocal
rem Acceptance test for C type names imported only through precompiled metadata: the same consumer
rem source must compile and run with the C header (Shared) and without it (Metadata), O0-O3.
rem The producer declares its header with PublicTypes="Header" (T08-D25), so the library publishes NAlias and
rem NMood although no public signature uses them.
if not defined NSC_T08_BUILD set "NSC_T08_BUILD=A:\nsc-interop-build\tanda1-ON"
if not defined NSC_T08_OUT set "NSC_T08_OUT=A:\nsc-interop-build\tanda08-nombres-metadata"
if not defined NSC_T08_NSLINK set "NSC_T08_NSLINK=A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
set "DEST=%NSC_T08_OUT%"
set "PATH=A:\al82dev-clang\bin;%NSC_T08_BUILD%\bin;%PATH%"
if not exist "%DEST%" mkdir "%DEST%"
if errorlevel 1 exit /b 1
set "TMP=%DEST%"
set "TEMP=%DEST%"
set "NSC_LINKER=%NSC_T08_NSLINK%"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=%NSC_T08_BUILD%\generated\NativeSharp.Runtime.Win32.lib"
set "CLANG_ROOT=--cpp-clang-root=A:\al82dev-clang"
set "EMIT_EXE=nsc.exe %CLANG_ROOT% --emit-project-oop-exe"
set "EMIT_LIB=nsc.exe %CLANG_ROOT% --emit-project-oop-lib"
set "FILES=Api.h Api.c Producer.ns Consumer.ns Producer.nsproj Shared.nsproj Metadata.nsproj"
set "FILES=%FILES% ProbeDecl.ns ProbeDecl.nsproj ProbeSizeof.ns ProbeSizeof.nsproj"
set "FILES=%FILES% ProbeCast.ns ProbeCast.nsproj ProbeValue.ns ProbeValue.nsproj"
for %%F in (%FILES%) do (
  copy /y "%~dp0%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 "%DEST%\Api.c" -o "%DEST%\api.obj"
if errorlevel 1 exit /b 1
llvm-lib.exe /nologo /out:"%DEST%\api.lib" "%DEST%\api.obj"
if errorlevel 1 exit /b 1
rem Per-position probes (metadata only, O0): they only print RESULTADO lines and never fail.
%EMIT_LIB% "%DEST%\Producer.nsproj" -O0 --no-artifact-cache -o "%DEST%\CNamesProducer.lib"
if errorlevel 1 exit /b 1
for %%P in (ProbeDecl ProbeSizeof ProbeCast ProbeValue) do call :probe %%P
for %%O in (0 1 2 3) do (
  %EMIT_LIB% "%DEST%\Producer.nsproj" -O%%O --no-artifact-cache -o "%DEST%\CNamesProducer.lib"
  if errorlevel 1 exit /b 1
  for %%P in (Shared Metadata) do (
    %EMIT_EXE% "%DEST%\%%P.nsproj" -O%%O --no-artifact-cache -o "%DEST%\%%P-O%%O.exe" > "%DEST%\%%P-O%%O.log" 2>&1
    if errorlevel 1 (
      type "%DEST%\%%P-O%%O.log"
      echo Nombres C %%P O%%O: compilacion rechazada.
      exit /b 1
    )
    "%DEST%\%%P-O%%O.exe"
    if errorlevel 1 exit /b 1
    echo Nombres C %%P O%%O: resultado esperado cero.
  )
)
exit /b 0
:probe
%EMIT_EXE% "%DEST%\%1.nsproj" -O0 --no-artifact-cache -o "%DEST%\%1.exe" > "%DEST%\%1.log" 2>&1
if errorlevel 1 (
  echo RESULTADO %1: compilacion rechazada, ver %DEST%\%1.log
  exit /b 0
)
"%DEST%\%1.exe"
echo RESULTADO %1: compila, codigo de salida %ERRORLEVEL%
exit /b 0
