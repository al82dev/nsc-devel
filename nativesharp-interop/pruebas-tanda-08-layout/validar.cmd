@echo off
setlocal
set "DEST=A:\nsc-interop-build\tanda08-layout"
set "PATH=A:\al82dev-clang\bin;A:\nsc-interop-build\tanda1-ON\bin;%PATH%"
if not exist "%DEST%" mkdir "%DEST%"
if errorlevel 1 exit /b 1
set "TMP=%DEST%"
set "TEMP=%DEST%"
set "NSC_LINKER=A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
for %%F in (Project.nsproj Main.ns Api.h Api.c) do (
  copy /y "%~dp0%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 "%DEST%\Api.c" -o "%DEST%\api.obj"
if errorlevel 1 exit /b 1
clang.exe -S -emit-llvm --target=x86_64-pc-windows-msvc -std=c11 "%DEST%\Api.c" -o "%DEST%\api.ll"
if errorlevel 1 exit /b 1
llvm-lib.exe /nologo /out:"%DEST%\api.lib" "%DEST%\api.obj"
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --dump-project-oop-llvm "%DEST%\Project.nsproj" > "%DEST%\project.ll"
if errorlevel 1 exit /b 1
python "%~dp0verificar_ir_campos.py" "%DEST%\project.ll"
if errorlevel 1 exit /b 1
python "%~dp0verificar_volatile_alineado.py" "%DEST%\api.ll" "%DEST%\project.ll"
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O0 --no-artifact-cache -o "%DEST%\program.exe"
if errorlevel 1 exit /b 1
"%DEST%\program.exe"
if errorlevel 1 exit /b 1
for %%O in (1 2 3) do (
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O%%O --no-artifact-cache -o "%DEST%\program-O%%O.exe"
  if errorlevel 1 exit /b 1
  "%DEST%\program-O%%O.exe"
  if errorlevel 1 exit /b 1
)
copy /y "%~dp0PackedAddress.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O0 --no-artifact-cache -o "%DEST%\packed-address.exe" > "%DEST%\packed-address.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /i /c:"addressable storage" "%DEST%\packed-address.log" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0PackedPointerAddress.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O0 --no-artifact-cache -o "%DEST%\packed-pointer-address.exe" > "%DEST%\packed-pointer-address.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /i /c:"addressable storage" "%DEST%\packed-pointer-address.log" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0PackedDoubleAddress.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O0 --no-artifact-cache -o "%DEST%\packed-double-address.exe" > "%DEST%\packed-double-address.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /i /c:"addressable storage" "%DEST%\packed-double-address.log" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0PackedArrayAddress.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O0 --no-artifact-cache -o "%DEST%\packed-array-address.exe" > "%DEST%\packed-array-address.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /i /c:"addressable storage" "%DEST%\packed-array-address.log" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0PackedConstArrayWrite.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O0 --no-artifact-cache -o "%DEST%\packed-const-write.exe" > "%DEST%\packed-const-write.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /i /c:"not assignable" "%DEST%\packed-const-write.log" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0PackedVolatileArrayAddress.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O0 --no-artifact-cache -o "%DEST%\packed-volatile-address.exe" > "%DEST%\packed-volatile-address.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /i /c:"cannot take the address of volatile storage" "%DEST%\packed-volatile-address.log" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0PackedPointerArrayAddress.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O0 --no-artifact-cache -o "%DEST%\packed-pointer-array-address.exe" > "%DEST%\packed-pointer-array-address.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /i /c:"addressable storage" "%DEST%\packed-pointer-array-address.log" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0PackedNestedAddress.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O0 --no-artifact-cache -o "%DEST%\packed-nested-address.exe" > "%DEST%\packed-nested-address.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /i /c:"addressable storage" "%DEST%\packed-nested-address.log" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0PackedNestedArrayAddress.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O0 --no-artifact-cache -o "%DEST%\packed-nested-array-address.exe" > "%DEST%\packed-nested-array-address.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /i /c:"addressable storage" "%DEST%\packed-nested-array-address.log" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0PackedRecordArrayAddress.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O0 --no-artifact-cache -o "%DEST%\packed-record-array-address.exe" > "%DEST%\packed-record-array-address.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /i /c:"addressable storage" "%DEST%\packed-record-array-address.log" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0PackedRecordMatrixAddress.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O0 --no-artifact-cache -o "%DEST%\packed-record-matrix-address.exe" > "%DEST%\packed-record-matrix-address.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /i /c:"addressable storage" "%DEST%\packed-record-matrix-address.log" >nul
if errorlevel 1 exit /b 1
for %%F in (PackedEnumAddress PackedEnumArrayAddress) do (
  copy /y "%~dp0%%F.ns.txt" "%DEST%\Main.ns" >nul
  if errorlevel 1 exit /b 1
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O0 --no-artifact-cache -o "%DEST%\%%F.exe" > "%DEST%\%%F.log" 2>&1
  if not errorlevel 1 exit /b 1
  findstr /i /c:"addressable storage" "%DEST%\%%F.log" >nul
  if errorlevel 1 exit /b 1
)
for %%F in (PackedConstRecordWrite PackedConstEnumWrite) do (
  copy /y "%~dp0%%F.ns.txt" "%DEST%\Main.ns" >nul
  if errorlevel 1 exit /b 1
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O0 --no-artifact-cache -o "%DEST%\%%F.exe" > "%DEST%\%%F.log" 2>&1
  if not errorlevel 1 exit /b 1
  findstr /i /c:"not assignable" "%DEST%\%%F.log" >nul
  if errorlevel 1 exit /b 1
)
for %%F in (PackedConstRecordAddress PackedConstEnumAddress) do (
  copy /y "%~dp0%%F.ns.txt" "%DEST%\Main.ns" >nul
  if errorlevel 1 exit /b 1
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O0 --no-artifact-cache -o "%DEST%\%%F.exe" > "%DEST%\%%F.log" 2>&1
  if not errorlevel 1 exit /b 1
  rem T08-D06: the address of const C storage is a `T const*`; `int*` loses constness. The packed field also
  rem breaks the alignment contract. / T08-D06: la direccion de almacenamiento C const es `T const*`.
  findstr /i /c:"discard pointee constness" "%DEST%\%%F.log" >nul
  if errorlevel 1 exit /b 1
  if /i "%%F"=="PackedConstRecordAddress" findstr /i /c:"alignment, or address-space contract" "%DEST%\%%F.log" >nul
  if errorlevel 1 exit /b 1
)
for %%F in (AlignedConstRecordAddress AlignedConstEnumAddress) do (
  copy /y "%~dp0%%F.ns.txt" "%DEST%\Main.ns" >nul
  if errorlevel 1 exit /b 1
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O0 --no-artifact-cache -o "%DEST%\%%F.exe" > "%DEST%\%%F.log" 2>&1
  if not errorlevel 1 exit /b 1
  rem T08-D06: `int*` from the address of const C storage loses constness.
  findstr /i /c:"discard pointee constness" "%DEST%\%%F.log" >nul
  if errorlevel 1 exit /b 1
)
for %%F in (PackedVolatileRecordAddress PackedVolatileEnumAddress) do (
  copy /y "%~dp0%%F.ns.txt" "%DEST%\Main.ns" >nul
  if errorlevel 1 exit /b 1
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O0 --no-artifact-cache -o "%DEST%\%%F.exe" > "%DEST%\%%F.log" 2>&1
  if not errorlevel 1 exit /b 1
  findstr /i /c:"cannot take the address of volatile storage" "%DEST%\%%F.log" >nul
  if errorlevel 1 exit /b 1
)
for %%F in (PackedConstVolatileRecordWrite PackedConstVolatileEnumWrite) do (
  copy /y "%~dp0%%F.ns.txt" "%DEST%\Main.ns" >nul
  if errorlevel 1 exit /b 1
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O0 --no-artifact-cache -o "%DEST%\%%F.exe" > "%DEST%\%%F.log" 2>&1
  if not errorlevel 1 exit /b 1
  findstr /i /c:"not assignable" "%DEST%\%%F.log" >nul
  if errorlevel 1 exit /b 1
)
for %%F in (PackedConstVolatileRecordAddress PackedConstVolatileEnumAddress) do (
  copy /y "%~dp0%%F.ns.txt" "%DEST%\Main.ns" >nul
  if errorlevel 1 exit /b 1
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O0 --no-artifact-cache -o "%DEST%\%%F.exe" > "%DEST%\%%F.log" 2>&1
  if not errorlevel 1 exit /b 1
  findstr /i /c:"cannot take the address of volatile storage" "%DEST%\%%F.log" >nul
  if errorlevel 1 exit /b 1
)
echo Layout C comparado: resultado esperado cero.
exit /b 0
