@echo off
setlocal
rem Exploratory probes: records whether each C layout form is accepted and executes correctly.
rem It never fails on a probe result; it prints one RESULTADO line per probe. Same overrides as validar.cmd.
if not defined NSC_T08_BUILD set "NSC_T08_BUILD=A:\nsc-interop-build\tanda1-ON"
if not defined NSC_T08_OUT set "NSC_T08_OUT=A:\nsc-interop-build\tanda08-layout-clang"
if not defined NSC_T08_NSLINK set "NSC_T08_NSLINK=A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
set "DEST=%NSC_T08_OUT%\formas"
set "PATH=A:\al82dev-clang\bin;%NSC_T08_BUILD%\bin;%PATH%"
if not exist "%DEST%" mkdir "%DEST%"
if errorlevel 1 exit /b 1
set "TMP=%DEST%"
set "TEMP=%DEST%"
set "NSC_LINKER=%NSC_T08_NSLINK%"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=%NSC_T08_BUILD%\generated\NativeSharp.Runtime.Win32.lib"
for %%F in (ApiProbes.c ProbeAnonymous.h ProbeNested.h ProbeFlex.h) do (
  copy /y "%~dp0%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 "%DEST%\ApiProbes.c" -o "%DEST%\probes.obj"
if errorlevel 1 exit /b 1
llvm-lib.exe /nologo /out:"%DEST%\probes.lib" "%DEST%\probes.obj"
if errorlevel 1 exit /b 1
for %%P in (Anonymous Nested FlexHeader FlexData) do call :probe %%P
exit /b 0
:probe
copy /y "%~dp0Probe%1.ns" "%DEST%\Probe%1.ns" >nul
copy /y "%~dp0Probe%1.nsproj" "%DEST%\Probe%1.nsproj" >nul
set "EMIT_EXE=nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe"
%EMIT_EXE% "%DEST%\Probe%1.nsproj" -O0 --no-artifact-cache -o "%DEST%\Probe%1.exe" > "%DEST%\Probe%1.log" 2>&1
if errorlevel 1 (
  echo RESULTADO Probe%1: compilacion rechazada, ver %DEST%\Probe%1.log
  exit /b 0
)
"%DEST%\Probe%1.exe"
echo RESULTADO Probe%1: compila, codigo de salida %ERRORLEVEL%
exit /b 0
