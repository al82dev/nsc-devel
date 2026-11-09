@echo off
setlocal
set "DEST=A:\nsc-interop-build\tanda08-tls-atomic-%RANDOM%-%RANDOM%"
if exist "%DEST%" exit /b 1
mkdir "%DEST%"
if errorlevel 1 goto :failure
set "PATH=A:\nsc-interop-build\tanda1-ON\bin;A:\al82dev-clang\bin;%PATH%"
set "NSC_LINKER=A:\al82dev-clang\bin\lld-link.exe"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
set "TMP=%DEST%"
set "TEMP=%DEST%"
for %%F in (Api.h Api.c Probe.c Promotion.c Promotion.ns Narrow.ns NarrowCompound.ns LocalNarrow.ns FloatCompound.ns FloatConstWrite.ns BoolConstWrite.ns PointerConstWrite.ns PointerQualified.ns PointerPointeeConstWrite.ns PointerPointeeConstEscape.ns PointerPointeeVolatile.ns PointerPointeeVolatileEscape.ns PointerCompound.ns PointerEscape.ns UnsignedPointer.ns UnsignedPointerQualified.ns UnsignedPointeeConstWrite.ns UnsignedPointeeConstEscape.ns UnsignedPointeeVolatileEscape.ns AdditionalPointers.ns AdditionalQualifiedPointers.ns AdditionalConstWrite.ns AdditionalVolatileEscape.ns UnsupportedQualifiedPointer.ns Main.ns Project.nsproj Escape.ns Unsupported.ns ReadonlyWrite.ns RefEscape.ns OutEscape.ns InEscape.ns ReferenceControl.ns ZeroDiv.ns ZeroRem.ns) do (
  copy /y "%~dp0%%F" "%DEST%\%%F" >nul
  if errorlevel 1 goto :failure
)
copy /y "%~dp0..\static-global\Crt.h" "%DEST%\Crt.h" >nul
if errorlevel 1 goto :failure
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 "%DEST%\Api.c" -o "%DEST%\api.obj"
if errorlevel 1 goto :failure
llvm-lib.exe /nologo /out:"%DEST%\api.lib" "%DEST%\api.obj"
if errorlevel 1 goto :failure
clang.exe -S -emit-llvm --target=x86_64-pc-windows-msvc -std=c11 -O0 "%DEST%\Probe.c" -o "%DEST%\probe.ll"
if errorlevel 1 goto :failure
python "%~dp0verificar_ir.py" "%DEST%\probe.ll" clang
if errorlevel 1 goto :failure
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 "%DEST%\Probe.c" -o "%DEST%\probe.obj"
if errorlevel 1 goto :failure
lld-link.exe /nologo /nodefaultlib /subsystem:console /entry:entry /out:"%DEST%\probe.exe" "%DEST%\probe.obj" "%DEST%\api.lib" "A:\vc2026-runtime\lib\x64\release\libcmt.lib" "A:\vc2026-runtime\lib\x64\release\libcpmt.lib" "A:\vc2026-runtime\lib\x64\release\libvcruntime.lib" "A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\ucrt\x64\libucrt.lib" "A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\um\x64\kernel32.lib"
if errorlevel 1 goto :failure
"%DEST%\probe.exe"
if errorlevel 1 goto :failure
for %%O in (0 1 2 3) do (
  clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 -O%%O "%DEST%\Promotion.c" -o "%DEST%\promotion-O%%O.obj"
  if errorlevel 1 goto :failure
  lld-link.exe /nologo /nodefaultlib /subsystem:console /entry:entry /include:_tls_used /out:"%DEST%\promotion-O%%O.exe" "%DEST%\promotion-O%%O.obj" "A:\vc2026-runtime\lib\x64\release\libcmt.lib" "A:\vc2026-runtime\windows-sdk\10.0.28000.0\lib\um\x64\kernel32.lib"
  if errorlevel 1 goto :failure
  "%DEST%\promotion-O%%O.exe"
  if errorlevel 1 (echo Control C de promocion atomica O%%O fallo. & exit /b 1)
)
nsc.exe --cpp-clang-root=A:\al82dev-clang --dump-project-oop-llvm "%DEST%\Project.nsproj" > "%DEST%\native.ll" 2> "%DEST%\native-ir.log"
if errorlevel 1 goto :failure
python "%~dp0verificar_ir.py" "%DEST%\native.ll" native
if errorlevel 1 goto :failure
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" --artifact-cache="%DEST%\cache" -O0 -o "%DEST%\cached.exe" > "%DEST%\cache.log" 2>&1
rem T08-D20: since 03bf12e42 a C/C++ project builds with the artifact cache: the cached build runs and the second
rem one hits the cache. / T08-D20: desde 03bf12e42 un proyecto C/C++ compila con la cache de artefactos.
if errorlevel 1 goto :failure
"%DEST%\cached.exe"
if errorlevel 1 goto :failure
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" --artifact-cache="%DEST%\cache" -O0 --time -o "%DEST%\cached.exe" > "%DEST%\cache-hit.log" 2>&1
if errorlevel 1 goto :failure
findstr /L /C:"compilation-bundle-cache: hit" "%DEST%\cache-hit.log" >nul
if errorlevel 1 goto :failure
for %%O in (0 1 2 3) do (
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" --no-artifact-cache -O%%O -o "%DEST%\native-O%%O.exe" > "%DEST%\native-O%%O.log" 2>&1
  if errorlevel 1 goto :failure
  "%DEST%\native-O%%O.exe"
  if errorlevel 1 goto :failure
)
for %%F in (ZeroDiv ZeroRem) do (
  copy /y "%DEST%\%%F.ns" "%DEST%\Main.ns" >nul
  if errorlevel 1 goto :failure
  for %%O in (0 1 2 3) do (
    nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" --no-artifact-cache -O%%O -o "%DEST%\%%F-O%%O.exe" > "%DEST%\%%F-O%%O.log" 2>&1
    if errorlevel 1 goto :failure
    "%DEST%\%%F-O%%O.exe" > "%DEST%\%%F-O%%O-run.log" 2>&1
    if not errorlevel 129 exit /b 1
    if errorlevel 130 exit /b 1
  )
)
copy /y "%DEST%\Narrow.ns" "%DEST%\Main.ns" >nul
if errorlevel 1 goto :failure
nsc.exe --cpp-clang-root=A:\al82dev-clang --dump-project-oop-llvm "%DEST%\Project.nsproj" > "%DEST%\narrow.ll" 2> "%DEST%\narrow-ir.log"
if errorlevel 1 goto :failure
python "%~dp0verificar_narrow.py" "%DEST%\narrow.ll"
if errorlevel 1 goto :failure
for %%O in (0 1 2 3) do (
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" --no-artifact-cache -O%%O -o "%DEST%\narrow-O%%O.exe" > "%DEST%\narrow-O%%O.log" 2>&1
  if errorlevel 1 goto :failure
  "%DEST%\narrow-O%%O.exe"
  if errorlevel 1 goto :failure
)
for %%F in (NarrowCompound Promotion FloatCompound PointerCompound PointerQualified PointerPointeeVolatile UnsignedPointer UnsignedPointerQualified AdditionalPointers AdditionalQualifiedPointers) do (
  copy /y "%DEST%\%%F.ns" "%DEST%\Main.ns" >nul
  if errorlevel 1 goto :failure
  for %%O in (0 1 2 3) do (
    nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" --no-artifact-cache -O%%O -o "%DEST%\%%F-O%%O.exe" > "%DEST%\%%F-O%%O.log" 2>&1
    if errorlevel 1 goto :failure
    "%DEST%\%%F-O%%O.exe"
    if errorlevel 1 goto :failure
  )
)
copy /y "%DEST%\UnsignedPointer.ns" "%DEST%\Main.ns" >nul
if errorlevel 1 goto :failure
nsc.exe --cpp-clang-root=A:\al82dev-clang --dump-project-oop-llvm "%DEST%\Project.nsproj" > "%DEST%\unsigned-pointer.ll" 2> "%DEST%\unsigned-pointer-ir.log"
if errorlevel 1 goto :failure
python "%~dp0verificar_uint_pointer.py" "%DEST%\unsigned-pointer.ll"
if errorlevel 1 goto :failure
copy /y "%DEST%\UnsignedPointerQualified.ns" "%DEST%\Main.ns" >nul
if errorlevel 1 goto :failure
nsc.exe --cpp-clang-root=A:\al82dev-clang --dump-project-oop-llvm "%DEST%\Project.nsproj" > "%DEST%\unsigned-qualified.ll" 2> "%DEST%\unsigned-qualified-ir.log"
if errorlevel 1 goto :failure
python "%~dp0verificar_uint_calificado.py" "%DEST%\unsigned-qualified.ll"
if errorlevel 1 goto :failure
copy /y "%DEST%\AdditionalPointers.ns" "%DEST%\Main.ns" >nul
if errorlevel 1 goto :failure
nsc.exe --cpp-clang-root=A:\al82dev-clang --dump-project-oop-llvm "%DEST%\Project.nsproj" > "%DEST%\additional-pointers.ll" 2> "%DEST%\additional-pointers-ir.log"
if errorlevel 1 goto :failure
python "%~dp0verificar_punteros_adicionales.py" "%DEST%\additional-pointers.ll"
if errorlevel 1 goto :failure
copy /y "%DEST%\AdditionalQualifiedPointers.ns" "%DEST%\Main.ns" >nul
if errorlevel 1 goto :failure
nsc.exe --cpp-clang-root=A:\al82dev-clang --dump-project-oop-llvm "%DEST%\Project.nsproj" > "%DEST%\additional-qualified.ll" 2> "%DEST%\additional-qualified-ir.log"
if errorlevel 1 goto :failure
python "%~dp0verificar_calificados_adicionales.py" "%DEST%\additional-qualified.ll"
if errorlevel 1 goto :failure
copy /y "%DEST%\NarrowCompound.ns" "%DEST%\Main.ns" >nul
if errorlevel 1 goto :failure
nsc.exe --cpp-clang-root=A:\al82dev-clang --dump-project-oop-llvm "%DEST%\Project.nsproj" > "%DEST%\narrow-compound.ll" 2> "%DEST%\narrow-compound-ir.log"
if errorlevel 1 goto :failure
python "%~dp0verificar_incremento.py" "%DEST%\narrow-compound.ll"
if errorlevel 1 goto :failure
copy /y "%DEST%\LocalNarrow.ns" "%DEST%\Main.ns" >nul
if errorlevel 1 goto :failure
nsc.exe --cpp-clang-root=A:\al82dev-clang --check-project-method-bodies "%DEST%\Project.nsproj" > "%DEST%\local-narrow.log" 2>&1
if not errorlevel 1 goto :failure
findstr /L /C:"compound assignment cannot implicitly convert" "%DEST%\local-narrow.log" >nul
if errorlevel 1 goto :failure
copy /y "%DEST%\Escape.ns" "%DEST%\Main.ns" >nul
if errorlevel 1 goto :failure
nsc.exe --cpp-clang-root=A:\al82dev-clang --check-project-method-bodies "%DEST%\Project.nsproj" > "%DEST%\escape.log" 2>&1
if not errorlevel 1 goto :failure
findstr /L /C:"atomic-qualified pointers" "%DEST%\escape.log" >nul
if errorlevel 1 goto :failure
copy /y "%DEST%\PointerEscape.ns" "%DEST%\Main.ns" >nul
if errorlevel 1 goto :failure
nsc.exe --cpp-clang-root=A:\al82dev-clang --check-project-method-bodies "%DEST%\Project.nsproj" > "%DEST%\pointer-escape.log" 2>&1
if not errorlevel 1 goto :failure
findstr /L /C:"atomic-qualified pointers" "%DEST%\pointer-escape.log" >nul
if errorlevel 1 goto :failure
copy /y "%DEST%\PointerPointeeConstWrite.ns" "%DEST%\Main.ns" >nul
if errorlevel 1 goto :failure
nsc.exe --cpp-clang-root=A:\al82dev-clang --check-project-method-bodies "%DEST%\Project.nsproj" > "%DEST%\pointer-pointee-const-write.log" 2>&1
if not errorlevel 1 goto :failure
findstr /L /C:"left side of assignment is not assignable" "%DEST%\pointer-pointee-const-write.log" >nul
if errorlevel 1 goto :failure
copy /y "%DEST%\PointerPointeeConstEscape.ns" "%DEST%\Main.ns" >nul
if errorlevel 1 goto :failure
nsc.exe --cpp-clang-root=A:\al82dev-clang --check-project-method-bodies "%DEST%\Project.nsproj" > "%DEST%\pointer-pointee-const-escape.log" 2>&1
if not errorlevel 1 goto :failure
findstr /L /C:"would discard pointee constness" "%DEST%\pointer-pointee-const-escape.log" >nul
if errorlevel 1 goto :failure
copy /y "%DEST%\PointerPointeeVolatile.ns" "%DEST%\Main.ns" >nul
if errorlevel 1 goto :failure
nsc.exe --cpp-clang-root=A:\al82dev-clang --dump-project-oop-llvm "%DEST%\Project.nsproj" > "%DEST%\pointer-pointee-volatile.ll" 2> "%DEST%\pointer-pointee-volatile-ir.log"
if errorlevel 1 goto :failure
findstr /C:"store atomic volatile i32" "%DEST%\pointer-pointee-volatile.ll" >nul
if errorlevel 1 goto :failure
findstr /C:"load atomic volatile i32" "%DEST%\pointer-pointee-volatile.ll" >nul
if errorlevel 1 goto :failure
copy /y "%DEST%\PointerPointeeVolatileEscape.ns" "%DEST%\Main.ns" >nul
if errorlevel 1 goto :failure
nsc.exe --cpp-clang-root=A:\al82dev-clang --check-project-method-bodies "%DEST%\Project.nsproj" > "%DEST%\pointer-pointee-volatile-escape.log" 2>&1
if not errorlevel 1 goto :failure
findstr /L /C:"would discard pointer pointee volatile qualification" "%DEST%\pointer-pointee-volatile-escape.log" >nul
if errorlevel 1 goto :failure
for %%F in (UnsignedPointeeConstWrite UnsignedPointeeConstEscape UnsignedPointeeVolatileEscape) do (
  copy /y "%DEST%\%%F.ns" "%DEST%\Main.ns" >nul
  if errorlevel 1 goto :failure
  nsc.exe --cpp-clang-root=A:\al82dev-clang --check-project-method-bodies "%DEST%\Project.nsproj" > "%DEST%\%%F.log" 2>&1
  if not errorlevel 1 goto :failure
)
findstr /L /C:"left side of assignment is not assignable" "%DEST%\UnsignedPointeeConstWrite.log" >nul
if errorlevel 1 goto :failure
findstr /L /C:"would discard pointee constness" "%DEST%\UnsignedPointeeConstEscape.log" >nul
if errorlevel 1 goto :failure
findstr /L /C:"would discard pointer pointee volatile qualification" "%DEST%\UnsignedPointeeVolatileEscape.log" >nul
if errorlevel 1 goto :failure
for %%F in (AdditionalConstWrite AdditionalVolatileEscape) do (
  copy /y "%DEST%\%%F.ns" "%DEST%\Main.ns" >nul
  if errorlevel 1 goto :failure
  nsc.exe --cpp-clang-root=A:\al82dev-clang --check-project-method-bodies "%DEST%\Project.nsproj" > "%DEST%\%%F.log" 2>&1
  if not errorlevel 1 goto :failure
)
findstr /L /C:"left side of assignment is not assignable" "%DEST%\AdditionalConstWrite.log" >nul
if errorlevel 1 goto :failure
findstr /L /C:"would discard pointer pointee volatile qualification" "%DEST%\AdditionalVolatileEscape.log" >nul
if errorlevel 1 goto :failure
python "%~dp0verificar_rechazos_adicionales.py" "%DEST%\AdditionalConstWrite.log" "%DEST%\AdditionalVolatileEscape.log"
if errorlevel 1 goto :failure
copy /y "%DEST%\ReferenceControl.ns" "%DEST%\Main.ns" >nul
if errorlevel 1 goto :failure
nsc.exe --cpp-clang-root=A:\al82dev-clang --check-project-method-bodies "%DEST%\Project.nsproj" > "%DEST%\reference-control.log" 2>&1
if errorlevel 1 goto :failure
for %%F in (RefEscape OutEscape InEscape) do (
  copy /y "%DEST%\%%F.ns" "%DEST%\Main.ns" >nul
  if errorlevel 1 goto :failure
  nsc.exe --cpp-clang-root=A:\al82dev-clang --check-project-method-bodies "%DEST%\Project.nsproj" > "%DEST%\%%F.log" 2>&1
  if not errorlevel 1 goto :failure
  findstr /L /C:"atomic-qualified pointers" "%DEST%\%%F.log" >nul
  if errorlevel 1 goto :failure
)
copy /y "%DEST%\ReadonlyWrite.ns" "%DEST%\Main.ns" >nul
if errorlevel 1 goto :failure
nsc.exe --cpp-clang-root=A:\al82dev-clang --check-project-method-bodies "%DEST%\Project.nsproj" > "%DEST%\readonly.log" 2>&1
if not errorlevel 1 goto :failure
findstr /L /C:"left side of assignment is not assignable" "%DEST%\readonly.log" >nul
if errorlevel 1 goto :failure
copy /y "%DEST%\FloatConstWrite.ns" "%DEST%\Main.ns" >nul
if errorlevel 1 goto :failure
nsc.exe --cpp-clang-root=A:\al82dev-clang --check-project-method-bodies "%DEST%\Project.nsproj" > "%DEST%\float-readonly.log" 2>&1
if not errorlevel 1 goto :failure
findstr /L /C:"left side of assignment is not assignable" "%DEST%\float-readonly.log" >nul
if errorlevel 1 goto :failure
copy /y "%DEST%\BoolConstWrite.ns" "%DEST%\Main.ns" >nul
if errorlevel 1 goto :failure
nsc.exe --cpp-clang-root=A:\al82dev-clang --check-project-method-bodies "%DEST%\Project.nsproj" > "%DEST%\bool-readonly.log" 2>&1
if not errorlevel 1 goto :failure
findstr /L /C:"left side of assignment is not assignable" "%DEST%\bool-readonly.log" >nul
if errorlevel 1 goto :failure
copy /y "%DEST%\PointerConstWrite.ns" "%DEST%\Main.ns" >nul
if errorlevel 1 goto :failure
nsc.exe --cpp-clang-root=A:\al82dev-clang --check-project-method-bodies "%DEST%\Project.nsproj" > "%DEST%\pointer-readonly.log" 2>&1
if not errorlevel 1 goto :failure
findstr /L /C:"left side of assignment is not assignable" "%DEST%\pointer-readonly.log" >nul
if errorlevel 1 goto :failure
copy /y "%DEST%\Unsupported.ns" "%DEST%\Main.ns" >nul
if errorlevel 1 goto :failure
rem T08-D07: a thread-local `_Atomic` record is supported now (whole-value atomic access).
rem T08-D07: un registro `_Atomic` thread-local ya esta soportado (acceso atomico al valor completo).
nsc.exe --cpp-clang-root=A:\al82dev-clang --dump-project-oop-llvm "%DEST%\Project.nsproj" > "%DEST%\unsupported.log" 2>&1
if errorlevel 1 goto :failure
copy /y "%DEST%\UnsupportedQualifiedPointer.ns" "%DEST%\Main.ns" >nul
if errorlevel 1 goto :failure
nsc.exe --cpp-clang-root=A:\al82dev-clang --check-project-method-bodies "%DEST%\Project.nsproj" > "%DEST%\unsupported-qualified-pointer.log" 2>&1
if not errorlevel 1 goto :failure
findstr /L /C:"thread-local C/C++ global is not yet projected" "%DEST%\unsupported-qualified-pointer.log" >nul
if errorlevel 1 goto :failure
echo TLS atomico C y NativeSharp correcto.
exit /b 0
:failure
type "%DEST%\*.log"
exit /b 1
