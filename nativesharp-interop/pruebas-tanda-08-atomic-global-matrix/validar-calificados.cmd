@echo off
setlocal
set "DEST=A:\nsc-interop-build\tanda08-atomic-global-qualified-%RANDOM%-%RANDOM%"
if exist "%DEST%" exit /b 1
mkdir "%DEST%"
if errorlevel 1 goto :failure
set "PATH=A:\al82dev-clang\bin;A:\nsc-interop-build\tanda1-ON\bin;%PATH%"
set "NSC_LINKER=A:\al82dev-clang\bin\lld-link.exe"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
set "TMP=%DEST%"
set "TEMP=%DEST%"
set "SOURCE=%~dp0..\pruebas-tanda-08-tls-c\atomic"
python "%~dp0crear-api-no-tls.py" "%SOURCE%" "%DEST%"
if errorlevel 1 goto :failure
for %%F in (Project.nsproj AdditionalQualifiedPointers.ns UnsignedPointerQualified.ns AdditionalConstWrite.ns AdditionalVolatileEscape.ns UnsignedPointeeConstWrite.ns UnsignedPointeeConstEscape.ns UnsignedPointeeVolatileEscape.ns) do (
  copy /y "%SOURCE%\%%F" "%DEST%\%%F" >nul
  if errorlevel 1 goto :failure
)
copy /y "%SOURCE%\..\static-global\Crt.h" "%DEST%\Crt.h" >nul
if errorlevel 1 goto :failure
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 "%DEST%\Api.c" -o "%DEST%\api.obj"
if errorlevel 1 goto :failure
llvm-lib.exe /nologo /out:"%DEST%\api.lib" "%DEST%\api.obj"
if errorlevel 1 goto :failure
for %%F in (AdditionalQualifiedPointers UnsignedPointerQualified) do (
  copy /y "%DEST%\%%F.ns" "%DEST%\Main.ns" >nul
  if errorlevel 1 goto :failure
  nsc.exe --cpp-clang-root=A:\al82dev-clang --dump-project-oop-llvm "%DEST%\Project.nsproj" > "%DEST%\%%F.ll" 2> "%DEST%\%%F-ir.log"
  if errorlevel 1 goto :failure
  if "%%F"=="AdditionalQualifiedPointers" python "%SOURCE%\verificar_calificados_adicionales.py" "%DEST%\%%F.ll"
  if errorlevel 1 goto :failure
  if "%%F"=="UnsignedPointerQualified" python "%SOURCE%\verificar_uint_calificado.py" "%DEST%\%%F.ll"
  if errorlevel 1 goto :failure
  for %%O in (0 1 2 3) do (
    nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O%%O --no-artifact-cache -o "%DEST%\%%F-O%%O.exe" > "%DEST%\%%F-O%%O.log" 2>&1
    if errorlevel 1 goto :failure
    "%DEST%\%%F-O%%O.exe"
    if errorlevel 1 goto :failure
    echo Punteros atomicos globales %%F O%%O correctos.
  )
)
for %%F in (AdditionalConstWrite AdditionalVolatileEscape UnsignedPointeeConstWrite UnsignedPointeeConstEscape UnsignedPointeeVolatileEscape) do (
  copy /y "%DEST%\%%F.ns" "%DEST%\Main.ns" >nul
  if errorlevel 1 goto :failure
  nsc.exe --cpp-clang-root=A:\al82dev-clang --check-project-method-bodies "%DEST%\Project.nsproj" > "%DEST%\%%F.log" 2>&1
  if not errorlevel 1 goto :failure
)
python "%SOURCE%\verificar_rechazos_adicionales.py" "%DEST%\AdditionalConstWrite.log" "%DEST%\AdditionalVolatileEscape.log"
if errorlevel 1 goto :failure
findstr /L /C:"left side of assignment is not assignable" "%DEST%\UnsignedPointeeConstWrite.log" >nul
if errorlevel 1 goto :failure
findstr /L /C:"would discard pointee constness" "%DEST%\UnsignedPointeeConstEscape.log" >nul
if errorlevel 1 goto :failure
findstr /L /C:"would discard pointer pointee volatile qualification" "%DEST%\UnsignedPointeeVolatileEscape.log" >nul
if errorlevel 1 goto :failure
echo Calificadores de punteros atomicos globales verificados.
exit /b 0
:failure
type "%DEST%\*.log"
exit /b 1
