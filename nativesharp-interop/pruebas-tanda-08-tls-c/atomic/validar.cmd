@echo off
setlocal
set "DEST=A:\nsc-interop-build\tanda08-tls-atomic-%RANDOM%-%RANDOM%"
if exist "%DEST%" exit /b 1
mkdir "%DEST%"
if errorlevel 1 exit /b 1
set "PATH=A:\nsc-interop-build\tanda1-ON\bin;A:\al82dev-clang\bin;%PATH%"
set "NSC_LINKER=A:\al82dev-clang\bin\lld-link.exe"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
set "TMP=%DEST%"
set "TEMP=%DEST%"
for %%F in (Api.h Api.c Probe.c Promotion.c Promotion.ns Narrow.ns NarrowCompound.ns LocalNarrow.ns FloatCompound.ns FloatConstWrite.ns BoolConstWrite.ns PointerConstWrite.ns PointerQualified.ns PointerPointeeConstWrite.ns PointerPointeeConstEscape.ns PointerPointeeVolatile.ns PointerPointeeVolatileEscape.ns PointerCompound.ns PointerEscape.ns UnsignedPointer.ns UnsignedPointerQualified.ns UnsignedPointeeConstWrite.ns UnsignedPointeeConstEscape.ns UnsignedPointeeVolatileEscape.ns AdditionalPointers.ns AdditionalQualifiedPointers.ns AdditionalConstWrite.ns AdditionalVolatileEscape.ns UnsupportedQualifiedPointer.ns Main.ns Project.nsproj Escape.ns Unsupported.ns ReadonlyWrite.ns RefEscape.ns OutEscape.ns InEscape.ns ReferenceControl.ns ZeroDiv.ns ZeroRem.ns) do (
  copy /y "%~dp0%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
copy /y "%~dp0..\static-global\Crt.h" "%DEST%\Crt.h" >nul
if errorlevel 1 exit /b 1
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 "%DEST%\Api.c" -o "%DEST%\api.obj"
if errorlevel 1 exit /b 1
llvm-lib.exe /nologo /out:"%DEST%\api.lib" "%DEST%\api.obj"
if errorlevel 1 exit /b 1
clang.exe -S -emit-llvm --target=x86_64-pc-windows-msvc -std=c11 -O0 "%DEST%\Probe.c" -o "%DEST%\probe.ll"
if errorlevel 1 exit /b 1
python "%~dp0verificar_ir.py" "%DEST%\probe.ll" clang
if errorlevel 1 exit /b 1
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 "%DEST%\Probe.c" -o "%DEST%\probe.obj"
if errorlevel 1 exit /b 1
lld-link.exe /nologo /nodefaultlib /subsystem:console /entry:entry /out:"%DEST%\probe.exe" "%DEST%\probe.obj" "%DEST%\api.lib" "A:\vc2026-runtime\lib\x64\release\libcmt.lib" "A:\vc2026-runtime\lib\x64\release\libcpmt.lib" "A:\vc2026-runtime\lib\x64\release\libvcruntime.lib" "A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\ucrt\x64\libucrt.lib" "A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\um\x64\kernel32.lib"
if errorlevel 1 exit /b 1
"%DEST%\probe.exe"
if errorlevel 1 exit /b 1
for %%O in (0 1 2 3) do (
  clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 -O%%O "%DEST%\Promotion.c" -o "%DEST%\promotion-O%%O.obj"
  if errorlevel 1 exit /b 1
  lld-link.exe /nologo /nodefaultlib /subsystem:console /entry:entry /include:_tls_used /out:"%DEST%\promotion-O%%O.exe" "%DEST%\promotion-O%%O.obj" "A:\vc2026-runtime\lib\x64\release\libcmt.lib" "A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\um\x64\kernel32.lib"
  if errorlevel 1 exit /b 1
  "%DEST%\promotion-O%%O.exe"
  if errorlevel 1 (echo Control C de promocion atomica O%%O fallo. & exit /b 1)
)
nsc.exe --cpp-clang-root=A:\al82dev-clang --dump-project-oop-llvm "%DEST%\Project.nsproj" > "%DEST%\native.ll" 2> "%DEST%\native-ir.log"
if errorlevel 1 (type "%DEST%\native-ir.log" & exit /b 1)
python "%~dp0verificar_ir.py" "%DEST%\native.ll" native
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" --artifact-cache="%DEST%\cache" -O0 -o "%DEST%\cached.exe" > "%DEST%\cache.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /L /C:"C/C++ project references require --no-artifact-cache until transitive headers are tracked" "%DEST%\cache.log" >nul
if errorlevel 1 (type "%DEST%\cache.log" & exit /b 1)
for %%O in (0 1 2 3) do (
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" --no-artifact-cache -O%%O -o "%DEST%\native-O%%O.exe" > "%DEST%\native-O%%O.log" 2>&1
  if errorlevel 1 (type "%DEST%\native-O%%O.log" & exit /b 1)
  "%DEST%\native-O%%O.exe"
  if errorlevel 1 exit /b 1
)
for %%F in (ZeroDiv ZeroRem) do (
  copy /y "%DEST%\%%F.ns" "%DEST%\Main.ns" >nul
  if errorlevel 1 exit /b 1
  for %%O in (0 1 2 3) do (
    nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" --no-artifact-cache -O%%O -o "%DEST%\%%F-O%%O.exe" > "%DEST%\%%F-O%%O.log" 2>&1
    if errorlevel 1 (type "%DEST%\%%F-O%%O.log" & exit /b 1)
    "%DEST%\%%F-O%%O.exe" > "%DEST%\%%F-O%%O-run.log" 2>&1
    if not errorlevel 129 exit /b 1
    if errorlevel 130 exit /b 1
  )
)
copy /y "%DEST%\Narrow.ns" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --dump-project-oop-llvm "%DEST%\Project.nsproj" > "%DEST%\narrow.ll" 2> "%DEST%\narrow-ir.log"
if errorlevel 1 (type "%DEST%\narrow-ir.log" & exit /b 1)
python "%~dp0verificar_narrow.py" "%DEST%\narrow.ll"
if errorlevel 1 exit /b 1
for %%O in (0 1 2 3) do (
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" --no-artifact-cache -O%%O -o "%DEST%\narrow-O%%O.exe" > "%DEST%\narrow-O%%O.log" 2>&1
  if errorlevel 1 (type "%DEST%\narrow-O%%O.log" & exit /b 1)
  "%DEST%\narrow-O%%O.exe"
  if errorlevel 1 exit /b 1
)
for %%F in (NarrowCompound Promotion FloatCompound PointerCompound PointerQualified PointerPointeeVolatile UnsignedPointer UnsignedPointerQualified AdditionalPointers AdditionalQualifiedPointers) do (
  copy /y "%DEST%\%%F.ns" "%DEST%\Main.ns" >nul
  if errorlevel 1 exit /b 1
  for %%O in (0 1 2 3) do (
    nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" --no-artifact-cache -O%%O -o "%DEST%\%%F-O%%O.exe" > "%DEST%\%%F-O%%O.log" 2>&1
    if errorlevel 1 (type "%DEST%\%%F-O%%O.log" & exit /b 1)
    "%DEST%\%%F-O%%O.exe"
    if errorlevel 1 exit /b 1
  )
)
copy /y "%DEST%\UnsignedPointer.ns" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --dump-project-oop-llvm "%DEST%\Project.nsproj" > "%DEST%\unsigned-pointer.ll" 2> "%DEST%\unsigned-pointer-ir.log"
if errorlevel 1 (type "%DEST%\unsigned-pointer-ir.log" & exit /b 1)
python "%~dp0verificar_uint_pointer.py" "%DEST%\unsigned-pointer.ll"
if errorlevel 1 exit /b 1
copy /y "%DEST%\UnsignedPointerQualified.ns" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --dump-project-oop-llvm "%DEST%\Project.nsproj" > "%DEST%\unsigned-qualified.ll" 2> "%DEST%\unsigned-qualified-ir.log"
if errorlevel 1 (type "%DEST%\unsigned-qualified-ir.log" & exit /b 1)
python "%~dp0verificar_uint_calificado.py" "%DEST%\unsigned-qualified.ll"
if errorlevel 1 exit /b 1
copy /y "%DEST%\AdditionalPointers.ns" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --dump-project-oop-llvm "%DEST%\Project.nsproj" > "%DEST%\additional-pointers.ll" 2> "%DEST%\additional-pointers-ir.log"
if errorlevel 1 (type "%DEST%\additional-pointers-ir.log" & exit /b 1)
python "%~dp0verificar_punteros_adicionales.py" "%DEST%\additional-pointers.ll"
if errorlevel 1 exit /b 1
copy /y "%DEST%\AdditionalQualifiedPointers.ns" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --dump-project-oop-llvm "%DEST%\Project.nsproj" > "%DEST%\additional-qualified.ll" 2> "%DEST%\additional-qualified-ir.log"
if errorlevel 1 (type "%DEST%\additional-qualified-ir.log" & exit /b 1)
python "%~dp0verificar_calificados_adicionales.py" "%DEST%\additional-qualified.ll"
if errorlevel 1 exit /b 1
copy /y "%DEST%\NarrowCompound.ns" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --dump-project-oop-llvm "%DEST%\Project.nsproj" > "%DEST%\narrow-compound.ll" 2> "%DEST%\narrow-compound-ir.log"
if errorlevel 1 (type "%DEST%\narrow-compound-ir.log" & exit /b 1)
python "%~dp0verificar_incremento.py" "%DEST%\narrow-compound.ll"
if errorlevel 1 exit /b 1
copy /y "%DEST%\LocalNarrow.ns" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --check-project-method-bodies "%DEST%\Project.nsproj" > "%DEST%\local-narrow.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /L /C:"compound assignment cannot implicitly convert" "%DEST%\local-narrow.log" >nul
if errorlevel 1 (type "%DEST%\local-narrow.log" & exit /b 1)
copy /y "%DEST%\Escape.ns" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --check-project-method-bodies "%DEST%\Project.nsproj" > "%DEST%\escape.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /L /C:"atomic-qualified pointers" "%DEST%\escape.log" >nul
if errorlevel 1 (type "%DEST%\escape.log" & exit /b 1)
copy /y "%DEST%\PointerEscape.ns" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --check-project-method-bodies "%DEST%\Project.nsproj" > "%DEST%\pointer-escape.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /L /C:"atomic-qualified pointers" "%DEST%\pointer-escape.log" >nul
if errorlevel 1 (type "%DEST%\pointer-escape.log" & exit /b 1)
copy /y "%DEST%\PointerPointeeConstWrite.ns" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --check-project-method-bodies "%DEST%\Project.nsproj" > "%DEST%\pointer-pointee-const-write.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /L /C:"left side of assignment is not assignable" "%DEST%\pointer-pointee-const-write.log" >nul
if errorlevel 1 (type "%DEST%\pointer-pointee-const-write.log" & exit /b 1)
copy /y "%DEST%\PointerPointeeConstEscape.ns" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --check-project-method-bodies "%DEST%\Project.nsproj" > "%DEST%\pointer-pointee-const-escape.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /L /C:"would discard pointee constness" "%DEST%\pointer-pointee-const-escape.log" >nul
if errorlevel 1 (type "%DEST%\pointer-pointee-const-escape.log" & exit /b 1)
copy /y "%DEST%\PointerPointeeVolatile.ns" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --dump-project-oop-llvm "%DEST%\Project.nsproj" > "%DEST%\pointer-pointee-volatile.ll" 2> "%DEST%\pointer-pointee-volatile-ir.log"
if errorlevel 1 (type "%DEST%\pointer-pointee-volatile-ir.log" & exit /b 1)
findstr /C:"store atomic volatile i32" "%DEST%\pointer-pointee-volatile.ll" >nul
if errorlevel 1 exit /b 1
findstr /C:"load atomic volatile i32" "%DEST%\pointer-pointee-volatile.ll" >nul
if errorlevel 1 exit /b 1
copy /y "%DEST%\PointerPointeeVolatileEscape.ns" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --check-project-method-bodies "%DEST%\Project.nsproj" > "%DEST%\pointer-pointee-volatile-escape.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /L /C:"would discard pointer pointee volatile qualification" "%DEST%\pointer-pointee-volatile-escape.log" >nul
if errorlevel 1 (type "%DEST%\pointer-pointee-volatile-escape.log" & exit /b 1)
for %%F in (UnsignedPointeeConstWrite UnsignedPointeeConstEscape UnsignedPointeeVolatileEscape) do (
  copy /y "%DEST%\%%F.ns" "%DEST%\Main.ns" >nul
  if errorlevel 1 exit /b 1
  nsc.exe --cpp-clang-root=A:\al82dev-clang --check-project-method-bodies "%DEST%\Project.nsproj" > "%DEST%\%%F.log" 2>&1
  if not errorlevel 1 (type "%DEST%\%%F.log" & exit /b 1)
)
findstr /L /C:"left side of assignment is not assignable" "%DEST%\UnsignedPointeeConstWrite.log" >nul
if errorlevel 1 (type "%DEST%\UnsignedPointeeConstWrite.log" & exit /b 1)
findstr /L /C:"would discard pointee constness" "%DEST%\UnsignedPointeeConstEscape.log" >nul
if errorlevel 1 (type "%DEST%\UnsignedPointeeConstEscape.log" & exit /b 1)
findstr /L /C:"would discard pointer pointee volatile qualification" "%DEST%\UnsignedPointeeVolatileEscape.log" >nul
if errorlevel 1 (type "%DEST%\UnsignedPointeeVolatileEscape.log" & exit /b 1)
for %%F in (AdditionalConstWrite AdditionalVolatileEscape) do (
  copy /y "%DEST%\%%F.ns" "%DEST%\Main.ns" >nul
  if errorlevel 1 exit /b 1
  nsc.exe --cpp-clang-root=A:\al82dev-clang --check-project-method-bodies "%DEST%\Project.nsproj" > "%DEST%\%%F.log" 2>&1
  if not errorlevel 1 (type "%DEST%\%%F.log" & exit /b 1)
)
findstr /L /C:"left side of assignment is not assignable" "%DEST%\AdditionalConstWrite.log" >nul
if errorlevel 1 (type "%DEST%\AdditionalConstWrite.log" & exit /b 1)
findstr /L /C:"would discard pointer pointee volatile qualification" "%DEST%\AdditionalVolatileEscape.log" >nul
if errorlevel 1 (type "%DEST%\AdditionalVolatileEscape.log" & exit /b 1)
python "%~dp0verificar_rechazos_adicionales.py" "%DEST%\AdditionalConstWrite.log" "%DEST%\AdditionalVolatileEscape.log"
if errorlevel 1 exit /b 1
copy /y "%DEST%\ReferenceControl.ns" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --check-project-method-bodies "%DEST%\Project.nsproj" > "%DEST%\reference-control.log" 2>&1
if errorlevel 1 (type "%DEST%\reference-control.log" & exit /b 1)
for %%F in (RefEscape OutEscape InEscape) do (
  copy /y "%DEST%\%%F.ns" "%DEST%\Main.ns" >nul
  if errorlevel 1 exit /b 1
  nsc.exe --cpp-clang-root=A:\al82dev-clang --check-project-method-bodies "%DEST%\Project.nsproj" > "%DEST%\%%F.log" 2>&1
  if not errorlevel 1 (type "%DEST%\%%F.log" & exit /b 1)
  findstr /L /C:"atomic-qualified pointers" "%DEST%\%%F.log" >nul
  if errorlevel 1 (type "%DEST%\%%F.log" & exit /b 1)
)
copy /y "%DEST%\ReadonlyWrite.ns" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --check-project-method-bodies "%DEST%\Project.nsproj" > "%DEST%\readonly.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /L /C:"left side of assignment is not assignable" "%DEST%\readonly.log" >nul
if errorlevel 1 (type "%DEST%\readonly.log" & exit /b 1)
copy /y "%DEST%\FloatConstWrite.ns" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --check-project-method-bodies "%DEST%\Project.nsproj" > "%DEST%\float-readonly.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /L /C:"left side of assignment is not assignable" "%DEST%\float-readonly.log" >nul
if errorlevel 1 (type "%DEST%\float-readonly.log" & exit /b 1)
copy /y "%DEST%\BoolConstWrite.ns" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --check-project-method-bodies "%DEST%\Project.nsproj" > "%DEST%\bool-readonly.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /L /C:"left side of assignment is not assignable" "%DEST%\bool-readonly.log" >nul
if errorlevel 1 (type "%DEST%\bool-readonly.log" & exit /b 1)
copy /y "%DEST%\PointerConstWrite.ns" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --check-project-method-bodies "%DEST%\Project.nsproj" > "%DEST%\pointer-readonly.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /L /C:"left side of assignment is not assignable" "%DEST%\pointer-readonly.log" >nul
if errorlevel 1 (type "%DEST%\pointer-readonly.log" & exit /b 1)
copy /y "%DEST%\Unsupported.ns" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --dump-project-oop-llvm "%DEST%\Project.nsproj" > "%DEST%\unsupported.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /L /C:"thread-local C/C++ global is not yet projected" "%DEST%\unsupported.log" >nul
if errorlevel 1 (type "%DEST%\unsupported.log" & exit /b 1)
copy /y "%DEST%\UnsupportedQualifiedPointer.ns" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --check-project-method-bodies "%DEST%\Project.nsproj" > "%DEST%\unsupported-qualified-pointer.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /L /C:"thread-local C/C++ global is not yet projected" "%DEST%\unsupported-qualified-pointer.log" >nul
if errorlevel 1 (type "%DEST%\unsupported-qualified-pointer.log" & exit /b 1)
echo TLS atomico C y NativeSharp correcto.
exit /b 0
