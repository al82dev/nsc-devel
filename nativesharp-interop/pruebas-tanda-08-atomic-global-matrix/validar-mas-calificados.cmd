@echo off
setlocal
set "DEST=A:\nsc-interop-build\tanda08-atomic-more-qualified-%RANDOM%-%RANDOM%"
if exist "%DEST%" exit /b 1
mkdir "%DEST%"
if errorlevel 1 goto :failure
set "PATH=A:\al82dev-clang\bin;A:\nsc-interop-build\tanda1-ON\bin;%PATH%"
set "NSC_LINKER=A:\al82dev-clang\bin\lld-link.exe"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
set "TMP=%DEST%"
set "TEMP=%DEST%"
for %%F in (MoreQualified.h MoreQualified.c MoreQualified.ns MoreQualified.nsproj MoreQualifiedProbe.c MoreConstWrite.ns MoreConstEscape.ns MoreVolatileEscape.ns) do (
  copy /y "%~dp0%%F" "%DEST%\%%F" >nul
  if errorlevel 1 goto :failure
)
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 "%DEST%\MoreQualified.c" -o "%DEST%\more-qualified.obj"
if errorlevel 1 goto :failure
llvm-lib.exe /nologo /out:"%DEST%\more-qualified.lib" "%DEST%\more-qualified.obj"
if errorlevel 1 goto :failure
clang.exe -S -emit-llvm --target=x86_64-pc-windows-msvc -std=c11 -O0 "%DEST%\MoreQualifiedProbe.c" -o "%DEST%\clang.ll"
if errorlevel 1 goto :failure
python "%~dp0verificar_mas_calificados.py" "%DEST%\clang.ll" clang
if errorlevel 1 goto :failure
nsc.exe --cpp-clang-root=A:\al82dev-clang --dump-project-oop-llvm "%DEST%\MoreQualified.nsproj" > "%DEST%\native.ll" 2> "%DEST%\native-ir.log"
if errorlevel 1 goto :failure
python "%~dp0verificar_mas_calificados.py" "%DEST%\native.ll"
if errorlevel 1 goto :failure
for %%O in (0 1 2 3) do (
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\MoreQualified.nsproj" -O%%O --no-artifact-cache -o "%DEST%\consumer-O%%O.exe" > "%DEST%\consumer-O%%O.log" 2>&1
  if errorlevel 1 goto :failure
  "%DEST%\consumer-O%%O.exe"
  if errorlevel 1 goto :failure
  echo Apuntados atomicos calificados adicionales O%%O correctos.
)
for %%F in (MoreConstWrite MoreConstEscape MoreVolatileEscape) do (
  copy /y "%DEST%\%%F.ns" "%DEST%\MoreQualified.ns" >nul
  if errorlevel 1 goto :failure
  nsc.exe --cpp-clang-root=A:\al82dev-clang --check-project-method-bodies "%DEST%\MoreQualified.nsproj" > "%DEST%\%%F.log" 2>&1
  if not errorlevel 1 goto :failure
)
python "%~dp0verificar_mas_rechazos.py" "%DEST%\MoreConstWrite.log" "%DEST%\MoreConstEscape.log" "%DEST%\MoreVolatileEscape.log"
if errorlevel 1 goto :failure
exit /b 0
:failure
type "%DEST%\*.log"
exit /b 1
