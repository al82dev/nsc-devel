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
for %%F in (Api.h Api.c Probe.c Promotion.c Promotion.ns Narrow.ns NarrowCompound.ns LocalNarrow.ns Main.ns Project.nsproj Escape.ns Unsupported.ns ReadonlyWrite.ns RefEscape.ns OutEscape.ns InEscape.ns ReferenceControl.ns ZeroDiv.ns ZeroRem.ns) do (
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
for %%F in (NarrowCompound Promotion) do (
  copy /y "%DEST%\%%F.ns" "%DEST%\Main.ns" >nul
  if errorlevel 1 exit /b 1
  for %%O in (0 1 2 3) do (
    nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" --no-artifact-cache -O%%O -o "%DEST%\%%F-O%%O.exe" > "%DEST%\%%F-O%%O.log" 2>&1
    if errorlevel 1 (type "%DEST%\%%F-O%%O.log" & exit /b 1)
    "%DEST%\%%F-O%%O.exe"
    if errorlevel 1 exit /b 1
  )
)
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
copy /y "%DEST%\Unsupported.ns" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --dump-project-oop-llvm "%DEST%\Project.nsproj" > "%DEST%\unsupported.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /L /C:"thread-local C/C++ global is not yet projected" "%DEST%\unsupported.log" >nul
if errorlevel 1 (type "%DEST%\unsupported.log" & exit /b 1)
echo TLS atomico C y NativeSharp correcto.
exit /b 0
