@echo off
setlocal
rem Compares imported C struct/union layout and field accesses against Clang (T08-010/011/012/020).
rem Optional overrides: NSC_T08_BUILD (nsc build root), NSC_T08_OUT (results), NSC_T08_NSLINK (linker).
if not defined NSC_T08_BUILD set "NSC_T08_BUILD=A:\nsc-interop-build\tanda1-ON"
if not defined NSC_T08_OUT set "NSC_T08_OUT=A:\nsc-interop-build\tanda08-layout-clang"
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
set "FILES=Api.h Api.c Main.ns Access.ns Consumer.ns Producer.ns"
set "FILES=%FILES% Project.nsproj Producer.nsproj Metadata.nsproj Shared.nsproj"
for %%F in (%FILES%) do (
  copy /y "%~dp0%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 "%DEST%\Api.c" -o "%DEST%\api.obj"
if errorlevel 1 exit /b 1
clang.exe -S -emit-llvm -O1 --target=x86_64-pc-windows-msvc -std=c11 "%DEST%\Api.c" -o "%DEST%\api-ref.ll"
if errorlevel 1 exit /b 1
llvm-lib.exe /nologo /out:"%DEST%\api.lib" "%DEST%\api.obj"
if errorlevel 1 exit /b 1
nsc.exe %CLANG_ROOT% --dump-project-oop-llvm "%DEST%\Project.nsproj" > "%DEST%\project.ll"
if errorlevel 1 exit /b 1
python "%~dp0verificar_alineacion_clang.py" "%DEST%\api-ref.ll" "%DEST%\project.ll" "%DEST%\alineacion-directa.txt"
if errorlevel 1 exit /b 1
for %%O in (0 1 2 3) do (
  %EMIT_EXE% "%DEST%\Project.nsproj" -O%%O --no-artifact-cache -o "%DEST%\program-O%%O.exe"
  if errorlevel 1 exit /b 1
  "%DEST%\program-O%%O.exe"
  if errorlevel 1 exit /b 1
  echo Layout C directo O%%O: resultado esperado cero.
)
for %%O in (0 1 2 3) do (
  %EMIT_LIB% "%DEST%\Producer.nsproj" -O%%O --no-artifact-cache -o "%DEST%\CLayoutClangProducer.lib"
  if errorlevel 1 exit /b 1
  for %%P in (Shared Metadata) do (
    %EMIT_EXE% "%DEST%\%%P.nsproj" -O%%O --no-artifact-cache -o "%DEST%\%%P-O%%O.exe"
    if errorlevel 1 exit /b 1
    "%DEST%\%%P-O%%O.exe"
    if errorlevel 1 exit /b 1
    echo Layout C %%P O%%O: resultado esperado cero.
  )
)
for %%P in (Shared Metadata) do (
  nsc.exe %CLANG_ROOT% --dump-project-oop-llvm "%DEST%\%%P.nsproj" > "%DEST%\%%P.ll"
  if errorlevel 1 exit /b 1
  python "%~dp0verificar_alineacion_clang.py" "%DEST%\api-ref.ll" "%DEST%\%%P.ll" "%DEST%\alineacion-%%P.txt"
  if errorlevel 1 exit /b 1
)
echo Layout C comparado con Clang: resultado esperado cero.
exit /b 0
