@echo off
setlocal
set "DEST=A:\nsc-interop-build\tanda08-atomic-global-extra-validation-%RANDOM%-%RANDOM%"
if exist "%DEST%" exit /b 1
mkdir "%DEST%"
if errorlevel 1 exit /b 1
set "PATH=A:\al82dev-clang\bin;A:\nsc-interop-build\tanda1-ON\bin;%PATH%"
set "NSC_LINKER=A:\al82dev-clang\bin\lld-link.exe"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
set "TMP=%DEST%"
set "TEMP=%DEST%"
for %%F in (Extra.h Extra.c Extra.ns Extra.nsproj ExtraProbe.c ExtraConstWrite.ns ExtraConstEscape.ns ExtraVolatileEscape.ns) do (
  copy /y "%~dp0%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 "%DEST%\Extra.c" -o "%DEST%\extra.obj"
if errorlevel 1 exit /b 1
llvm-lib.exe /nologo /out:"%DEST%\extra.lib" "%DEST%\extra.obj"
if errorlevel 1 exit /b 1
clang.exe -S -emit-llvm --target=x86_64-pc-windows-msvc -std=c11 -O0 "%DEST%\ExtraProbe.c" -o "%DEST%\clang.ll"
if errorlevel 1 exit /b 1
python "%~dp0verificar_ir_extra.py" "%DEST%\clang.ll"
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --dump-project-oop-llvm "%DEST%\Extra.nsproj" > "%DEST%\native.ll" 2> "%DEST%\native-ir.log"
if errorlevel 1 (type "%DEST%\native-ir.log" & exit /b 1)
python "%~dp0verificar_ir_extra.py" "%DEST%\native.ll"
if errorlevel 1 exit /b 1
for %%O in (0 1 2 3) do (
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Extra.nsproj" -O%%O --no-artifact-cache -o "%DEST%\consumer-O%%O.exe" > "%DEST%\consumer-O%%O.log" 2>&1
  if errorlevel 1 (type "%DEST%\consumer-O%%O.log" & exit /b 1)
  "%DEST%\consumer-O%%O.exe"
  if errorlevel 1 exit /b 1
  echo Tipos atomicos globales adicionales O%%O correctos.
)
for %%F in (ExtraConstWrite ExtraConstEscape ExtraVolatileEscape) do (
  copy /y "%DEST%\%%F.ns" "%DEST%\Extra.ns" >nul
  if errorlevel 1 exit /b 1
  nsc.exe --cpp-clang-root=A:\al82dev-clang --check-project-method-bodies "%DEST%\Extra.nsproj" > "%DEST%\%%F.log" 2>&1
  if not errorlevel 1 (type "%DEST%\%%F.log" & exit /b 1)
)
findstr /L /C:"left side of assignment is not assignable" "%DEST%\ExtraConstWrite.log" >nul
if errorlevel 1 exit /b 1
findstr /L /C:"would discard pointee constness" "%DEST%\ExtraConstEscape.log" >nul
if errorlevel 1 exit /b 1
findstr /L /C:"would discard pointer pointee volatile qualification" "%DEST%\ExtraVolatileEscape.log" >nul
if errorlevel 1 exit /b 1
echo Calificadores de punteros globales preservados.
exit /b 0
