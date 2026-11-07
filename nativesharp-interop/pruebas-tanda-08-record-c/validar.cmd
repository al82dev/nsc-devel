@echo off
setlocal
set "DEST=A:\nsc-interop-build\tanda08-record-c"
set "PATH=A:\al82dev-clang\bin;A:\nsc-interop-build\tanda1-ON\bin;%PATH%"
if not exist "%DEST%" mkdir "%DEST%"
if errorlevel 1 exit /b 1
set "TMP=%DEST%"
set "TEMP=%DEST%"
set "NSC_LINKER=A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
copy /y "%~dp0Project.nsproj" "%DEST%\Project.nsproj" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0Main.ns" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0Api.h" "%DEST%\Api.h" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0Api.c" "%DEST%\Api.c" >nul
if errorlevel 1 exit /b 1
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 "%DEST%\Api.c" -o "%DEST%\api.obj"
if errorlevel 1 exit /b 1
clang.exe -S -emit-llvm -O0 --target=x86_64-pc-windows-msvc -std=c11 "%DEST%\Api.c" -o "%DEST%\api.ll"
if errorlevel 1 exit /b 1
python "%~dp0verificar_ir_c.py" "%DEST%\api.ll"
if errorlevel 1 exit /b 1
llvm-lib.exe /nologo /out:"%DEST%\api.lib" "%DEST%\api.obj"
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --dump-project-oop-llvm "%DEST%\Project.nsproj" > "%DEST%\project.ll"
if errorlevel 1 exit /b 1
python "%~dp0verificar_ir_coalescencia.py" "%DEST%\project.ll"
if errorlevel 1 exit /b 1
findstr /r /c:"load volatile.*%%field.addr" "%DEST%\project.ll" >nul
if errorlevel 1 (echo Falta lectura LLVM volatile del campo. & exit /b 1)
findstr /r /c:"store volatile.*%%field.addr" "%DEST%\project.ll" >nul
if errorlevel 1 (echo Falta escritura LLVM volatile del campo. & exit /b 1)
findstr /r /c:"load atomic volatile i32, ptr @shared_volatile acquire, align 4" "%DEST%\project.ll" >nul
if errorlevel 1 (echo Falta lectura volatile acquire del global. & exit /b 1)
findstr /r /c:"store atomic volatile i32 .*@shared_volatile release, align 4" "%DEST%\project.ll" >nul
if errorlevel 1 (echo Falta escritura volatile release del global. & exit /b 1)
for %%F in (project api) do (
  findstr /c:"load atomic volatile float, ptr @volatile_f32 acquire, align 4" "%DEST%\%%F.ll" >nul
  if errorlevel 1 (echo Falta lectura acquire float en %%F. & exit /b 1)
  findstr /r /c:"store atomic volatile float .*@volatile_f32 release, align 4" "%DEST%\%%F.ll" >nul
  if errorlevel 1 (echo Falta escritura release float en %%F. & exit /b 1)
  findstr /c:"load atomic volatile double, ptr @volatile_f64 acquire, align 8" "%DEST%\%%F.ll" >nul
  if errorlevel 1 (echo Falta lectura acquire double en %%F. & exit /b 1)
  findstr /r /c:"store atomic volatile double .*@volatile_f64 release, align 8" "%DEST%\%%F.ll" >nul
  if errorlevel 1 (echo Falta escritura release double en %%F. & exit /b 1)
)
for %%P in (i8:8:1 u8:8:1 i16:16:2 u16:16:2 i64:64:8 u64:64:8) do (
  for /f "tokens=1,2,3 delims=:" %%A in ("%%P") do (
    findstr /c:"load atomic volatile i%%B, ptr @volatile_%%A acquire, align %%C" "%DEST%\project.ll" >nul
    if errorlevel 1 (echo Falta lectura volatile acquire con ancho y alineacion de %%A. & exit /b 1)
    findstr /r /c:"store atomic volatile i%%B .*@volatile_%%A release, align %%C" "%DEST%\project.ll" >nul
    if errorlevel 1 (echo Falta escritura volatile release con ancho y alineacion de %%A. & exit /b 1)
  )
)
findstr /r /c:"store atomic volatile i32 61, ptr .*release, align 4" "%DEST%\project.ll" >nul
if errorlevel 1 (echo Falta escritura volatile release en el objeto apuntado. & exit /b 1)
findstr /r /c:"store volatile i32 69, ptr" "%DEST%\project.ll" >nul
if errorlevel 1 (echo Falta escritura volatile mediante alias tipado. & exit /b 1)
findstr /r /c:"store volatile i32 71, ptr" "%DEST%\project.ll" >nul
if errorlevel 1 (echo Falta escritura volatile despues de la conversion al alias. & exit /b 1)
findstr /r /c:"store volatile i32 72, ptr" "%DEST%\project.ll" >nul
if errorlevel 1 (echo Falta escritura volatile desde el global NativeSharp. & exit /b 1)
findstr /r /c:"store volatile i32 73, ptr" "%DEST%\project.ll" >nul
if errorlevel 1 (echo Falta escritura volatile desde el campo NativeSharp. & exit /b 1)
findstr /r /c:"store volatile i32 74, ptr" "%DEST%\project.ll" >nul
if errorlevel 1 (echo Falta escritura volatile mediante alias de struct. & exit /b 1)
findstr /r /c:"store volatile i32 67, ptr" "%DEST%\project.ll" >nul
if errorlevel 1 (echo Falta escritura volatile del campo apuntado. & exit /b 1)
findstr /r /c:"store volatile i32 68, ptr" "%DEST%\project.ll" >nul
if errorlevel 1 (echo Falta escritura volatile del campo del resultado C. & exit /b 1)
findstr /r /c:"store volatile i32 70, ptr" "%DEST%\project.ll" >nul
if errorlevel 1 (echo Falta escritura volatile del campo anidado apuntado. & exit /b 1)
findstr /r /c:"load atomic volatile.*@volatile_array.*acquire, align 4" "%DEST%\project.ll" >nul
if errorlevel 1 (echo Falta lectura acquire volatile del arreglo. & exit /b 1)
findstr /r /c:"store atomic volatile.*@volatile_array.*release, align 4" "%DEST%\project.ll" >nul
if errorlevel 1 (echo Falta escritura release volatile del arreglo. & exit /b 1)
findstr /r /c:"store atomic volatile i32 97, ptr .*release, align 4" "%DEST%\project.ll" >nul
if errorlevel 1 (echo Falta escritura release mediante puntero del arreglo. & exit /b 1)
findstr /r /c:"store atomic volatile i32 98, ptr .*release, align 4" "%DEST%\project.ll" >nul
if errorlevel 1 (echo Falta escritura release mediante puntero de la matriz. & exit /b 1)
findstr /r /c:"store atomic volatile i32 99, ptr .*release, align 4" "%DEST%\project.ll" >nul
if errorlevel 1 (echo Falta escritura release mediante puntero constante. & exit /b 1)
findstr /r /c:"store atomic volatile i32 103, ptr .*release, align 4" "%DEST%\project.ll" >nul
if errorlevel 1 (echo Falta escritura release por indice de puntero del arreglo. & exit /b 1)
findstr /r /c:"load atomic volatile i32, ptr %%pointer.load.*acquire, align 4" "%DEST%\project.ll" >nul
if errorlevel 1 (echo Falta lectura acquire mediante puntero del arreglo. & exit /b 1)
findstr /r /c:"load atomic volatile.*@volatile_pointer_array" "%DEST%\project.ll" >nul
if not errorlevel 1 (echo El slot del arreglo no debe ser volatile. & exit /b 1)
findstr /r /c:"store atomic volatile.*@volatile_pointer_array" "%DEST%\project.ll" >nul
if not errorlevel 1 (echo La escritura del slot no debe ser volatile. & exit /b 1)
findstr /r /c:"store.*volatile i32 101" "%DEST%\project.ll" >nul
if errorlevel 1 (echo Falta escritura volatile mediante puntero de struct. & exit /b 1)
findstr /r /c:"store atomic volatile i32 61, ptr .*release, align 4" "%DEST%\project.ll" >nul
if errorlevel 1 (echo Falta escritura release del global volatile int apuntado. & exit /b 1)
findstr /r /c:"load atomic volatile i32, ptr %%static.field.load.*acquire, align 4" "%DEST%\project.ll" >nul
if errorlevel 1 (echo Falta lectura acquire del global volatile int apuntado. & exit /b 1)
findstr /r /c:"store atomic volatile i32 102, ptr .*release, align 4" "%DEST%\project.ll" >nul
if errorlevel 1 (echo Falta escritura release por indice del puntero global. & exit /b 1)
findstr /r /c:"load atomic volatile i32, ptr %%raw.element.addr.*acquire, align 4" "%DEST%\project.ll" >nul
if errorlevel 1 (echo Falta lectura acquire por indice del puntero global. & exit /b 1)
findstr /r /c:"load atomic volatile.*@volatile_matrix.*acquire, align 4" "%DEST%\project.ll" >nul
if errorlevel 1 (echo Falta lectura acquire volatile de la matriz. & exit /b 1)
findstr /r /c:"store atomic volatile.*@volatile_matrix.*release, align 4" "%DEST%\project.ll" >nul
if errorlevel 1 (echo Falta escritura release volatile de la matriz. & exit /b 1)
findstr /r /c:"load atomic volatile i8.*@volatile_bool_array.*acquire, align 1" "%DEST%\project.ll" >nul
if errorlevel 1 (echo Falta lectura acquire de almacenamiento bool C. & exit /b 1)
findstr /r /c:"store atomic volatile i8.*@volatile_bool_array.*release, align 1" "%DEST%\project.ll" >nul
if errorlevel 1 (echo Falta escritura release de almacenamiento bool C. & exit /b 1)
for %%P in (i8:8:1 u8:8:1 i16:16:2 u16:16:2 u32:32:4 i64:64:8 u64:64:8) do (
  for /f "tokens=1,2,3 delims=:" %%A in ("%%P") do (
    findstr /r /c:"load atomic volatile i%%B.*@volatile_%%A_array.*acquire, align %%C" "%DEST%\project.ll" >nul
    if errorlevel 1 (echo Falta lectura acquire del arreglo %%A. & exit /b 1)
    findstr /r /c:"store atomic volatile i%%B.*@volatile_%%A_array.*release, align %%C" "%DEST%\project.ll" >nul
    if errorlevel 1 (echo Falta escritura release del arreglo %%A. & exit /b 1)
  )
)
findstr /r /c:"load atomic volatile float.*@volatile_f32_array.*acquire, align 4" "%DEST%\project.ll" >nul
if errorlevel 1 (echo Falta lectura acquire del arreglo float. & exit /b 1)
findstr /r /c:"store atomic volatile float.*@volatile_f32_array.*release, align 4" "%DEST%\project.ll" >nul
if errorlevel 1 (echo Falta escritura release del arreglo float. & exit /b 1)
findstr /r /c:"load atomic volatile double.*@volatile_f64_array.*acquire, align 8" "%DEST%\project.ll" >nul
if errorlevel 1 (echo Falta lectura acquire del arreglo double. & exit /b 1)
findstr /r /c:"store atomic volatile double.*@volatile_f64_array.*release, align 8" "%DEST%\project.ll" >nul
if errorlevel 1 (echo Falta escritura release del arreglo double. & exit /b 1)
findstr /r /c:"load volatile.*@global_record_volatile" "%DEST%\project.ll" >nul
if errorlevel 1 (echo Falta lectura LLVM volatile del campo global. & exit /b 1)
findstr /r /c:"store volatile.*@global_record_volatile" "%DEST%\project.ll" >nul
if errorlevel 1 (echo Falta escritura LLVM volatile del campo global. & exit /b 1)
findstr /r /c:"load volatile.*@volatile_record_matrix" "%DEST%\project.ll" >nul
if errorlevel 1 (echo Falta lectura volatile de matriz de structs. & exit /b 1)
findstr /r /c:"store volatile i32 43,.*@volatile_record_matrix" "%DEST%\project.ll" >nul
if errorlevel 1 (echo Falta escritura volatile de matriz de structs. & exit /b 1)
findstr /r /c:"store volatile i32 50," "%DEST%\project.ll" >nul
if errorlevel 1 (echo Falta escritura volatile de campo anidado. & exit /b 1)
findstr /c:"load volatile ptr, ptr getelementptr (i8, ptr @volatile_nested_record" "%DEST%\project.ll" >nul
if errorlevel 1 (echo Falta lectura volatile del campo puntero anidado. & exit /b 1)
findstr /r /c:"store i32 47, ptr" "%DEST%\project.ll" >nul
if errorlevel 1 (echo Falta escritura ordinaria por puntero de campo volatile. & exit /b 1)
findstr /r /c:"store volatile i32 47," "%DEST%\project.ll" >nul
if not errorlevel 1 (echo El campo puntero volatile califico incorrectamente el apuntado. & exit /b 1)
findstr /r /c:"store volatile i32 17," "%DEST%\project.ll" >nul
if not errorlevel 1 (echo Se propago volatile del puntero a su apuntado. & exit /b 1)
findstr /c:"load atomic volatile ptr, ptr @volatile_record_pointer acquire, align 8" "%DEST%\project.ll" >nul
if errorlevel 1 (echo Falta lectura volatile acquire del puntero. & exit /b 1)
findstr /r /c:"store atomic volatile ptr .*@volatile_record_pointer release, align 8" "%DEST%\project.ll" >nul
if errorlevel 1 (echo Falta escritura volatile release del puntero. & exit /b 1)
findstr /r /c:"load atomic volatile ptr, ptr @global_record_pointer" "%DEST%\project.ll" >nul
if not errorlevel 1 (echo Se califico incorrectamente el puntero ordinario. & exit /b 1)
findstr /r /c:"store i32 17, ptr" "%DEST%\project.ll" >nul
if errorlevel 1 (echo Falta escritura ordinaria en el objeto apuntado. & exit /b 1)
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O0 --no-artifact-cache -o "%DEST%\program.exe"
if errorlevel 1 exit /b 1
"%DEST%\program.exe"
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O1 --no-artifact-cache -o "%DEST%\program-o1.exe"
if errorlevel 1 exit /b 1
"%DEST%\program-o1.exe"
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O2 --no-artifact-cache -o "%DEST%\program-o2.exe"
if errorlevel 1 exit /b 1
"%DEST%\program-o2.exe"
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O3 --no-artifact-cache -o "%DEST%\program-o3.exe"
if errorlevel 1 exit /b 1
"%DEST%\program-o3.exe"
if errorlevel 1 exit /b 1
copy /y "%~dp0ArrayAssign.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O0 --no-artifact-cache -o "%DEST%\array-assign.exe" > "%DEST%\array-assign.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /i /c:"not assignable" /c:"not reassignable" "%DEST%\array-assign.log" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0ConstArrayWrite.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O0 --no-artifact-cache -o "%DEST%\const-array-write.exe" > "%DEST%\const-array-write.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /i /c:"not assignable" "%DEST%\const-array-write.log" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0ConstArrayAddress.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O0 --no-artifact-cache -o "%DEST%\const-array-address.exe" > "%DEST%\const-array-address.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /i /c:"const" /c:"readonly" "%DEST%\const-array-address.log" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0VolatileArrayAddress.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O0 --no-artifact-cache -o "%DEST%\volatile-array-address.exe" > "%DEST%\volatile-array-address.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /c:"cannot take the address of a volatile C/C++ array element" "%DEST%\volatile-array-address.log" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0VolatileMatrixAddress.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O0 --no-artifact-cache -o "%DEST%\volatile-matrix-address.exe" > "%DEST%\volatile-matrix-address.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /c:"cannot take the address of a volatile C/C++ array element" "%DEST%\volatile-matrix-address.log" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0ConstRecordGlobal.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O0 --no-artifact-cache -o "%DEST%\const-record.exe" > "%DEST%\const-record.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /c:"left side of assignment is not assignable" "%DEST%\const-record.log" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0ConstRecordAddress.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O0 --no-artifact-cache -o "%DEST%\const-record-address.exe" > "%DEST%\const-record-address.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /c:"cannot take the address of readonly storage until qualified pointers are supported" "%DEST%\const-record-address.log" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0ConstRecordPointerWrite.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O0 --no-artifact-cache -o "%DEST%\const-record-pointer-write.exe" > "%DEST%\const-record-pointer-write.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /c:"left side of assignment is not assignable" "%DEST%\const-record-pointer-write.log" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0ConstRecordPointerAddress.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O0 --no-artifact-cache -o "%DEST%\const-record-pointer-address.exe" > "%DEST%\const-record-pointer-address.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /c:"cannot take the address of readonly storage until qualified pointers are supported" "%DEST%\const-record-pointer-address.log" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0ConstRecordPointerConversion.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O0 --no-artifact-cache -o "%DEST%\const-record-pointer-conversion.exe" > "%DEST%\const-record-pointer-conversion.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /c:"would discard pointee constness" "%DEST%\const-record-pointer-conversion.log" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0ConstRecordPointerCall.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O0 --no-artifact-cache -o "%DEST%\const-record-pointer-call.exe" > "%DEST%\const-record-pointer-call.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /c:"no viable overload of namespace function 'write_mutable_globalonly'" "%DEST%\const-record-pointer-call.log" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0ConstRecordReturnWrite.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O0 --no-artifact-cache -o "%DEST%\const-record-return-write.exe" > "%DEST%\const-record-return-write.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /c:"left side of assignment is not assignable" "%DEST%\const-record-return-write.log" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0ConstRecordReturnConversion.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O0 --no-artifact-cache -o "%DEST%\const-record-return-conversion.exe" > "%DEST%\const-record-return-conversion.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /c:"would discard pointee constness" "%DEST%\const-record-return-conversion.log" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0ConstRecordReturnCall.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O0 --no-artifact-cache -o "%DEST%\const-record-return-call.exe" > "%DEST%\const-record-return-call.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /c:"no viable overload of namespace function 'write_mutable_globalonly'" "%DEST%\const-record-return-call.log" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0ConstScalarReturnWrite.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O0 --no-artifact-cache -o "%DEST%\const-scalar-return-write.exe" > "%DEST%\const-scalar-return-write.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /c:"left side of assignment is not assignable" "%DEST%\const-scalar-return-write.log" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0VolatileRecordGlobal.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O0 --no-artifact-cache -o "%DEST%\volatile-record.exe" > "%DEST%\volatile-record.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /c:"cannot take the address of volatile storage until qualified pointers are supported" "%DEST%\volatile-record.log" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0ConstRecordMatrixWrite.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O0 --no-artifact-cache -o "%DEST%\const-record-matrix-write.exe" > "%DEST%\const-record-matrix-write.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /c:"left side of assignment is not assignable" "%DEST%\const-record-matrix-write.log" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0ConstRecordMatrixAddress.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O0 --no-artifact-cache -o "%DEST%\const-record-matrix-address.exe" > "%DEST%\const-record-matrix-address.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /c:"cannot take the address of readonly storage until qualified pointers are supported" "%DEST%\const-record-matrix-address.log" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0VolatileRecordMatrixAddress.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O0 --no-artifact-cache -o "%DEST%\volatile-record-matrix-address.exe" > "%DEST%\volatile-record-matrix-address.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /c:"cannot take the address of volatile storage until qualified pointers are supported" "%DEST%\volatile-record-matrix-address.log" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0VolatileNestedRecordAddress.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O0 --no-artifact-cache -o "%DEST%\volatile-nested-record-address.exe" > "%DEST%\volatile-nested-record-address.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /c:"cannot take the address of volatile storage until qualified pointers are supported" "%DEST%\volatile-nested-record-address.log" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0ConstVolatileParameter.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O0 --no-artifact-cache -o "%DEST%\const-volatile-parameter.exe" > "%DEST%\const-volatile-parameter.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /c:"no viable overload of namespace function 'bump_volatile'" "%DEST%\const-volatile-parameter.log" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0ConstVolatileRecordParameter.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O0 --no-artifact-cache -o "%DEST%\const-volatile-record-parameter.exe" > "%DEST%\const-volatile-record-parameter.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /c:"no viable overload of namespace function 'bump_volatile_record'" "%DEST%\const-volatile-record-parameter.log" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0VolatileBoolArrayCompound.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
for %%O in (0 1 2 3) do (
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O%%O --no-artifact-cache -o "%DEST%\volatile-bool-compound-O%%O.exe" > "%DEST%\volatile-bool-compound-O%%O.log" 2>&1
  if errorlevel 1 (type "%DEST%\volatile-bool-compound-O%%O.log" & exit /b 1)
  "%DEST%\volatile-bool-compound-O%%O.exe"
  if errorlevel 1 exit /b 1
  echo C volatile bool compound bitwise O%%O correcto.
)
nsc.exe --cpp-clang-root=A:\al82dev-clang --dump-project-oop-llvm "%DEST%\Project.nsproj" > "%DEST%\volatile-bool-compound.ll" 2>&1
if errorlevel 1 exit /b 1
findstr /c:"c.bool.compound.load = load atomic volatile i8" "%DEST%\volatile-bool-compound.ll" >nul
if errorlevel 1 exit /b 1
findstr /c:"store atomic volatile i8" "%DEST%\volatile-bool-compound.ll" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0NativeBoolCompound.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O0 --no-artifact-cache -o "%DEST%\native-bool-compound.exe" > "%DEST%\native-bool-compound.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /c:"compound assignment requires compatible numeric operands" "%DEST%\native-bool-compound.log" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0VolatilePointerArrayDiscard.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O0 --no-artifact-cache -o "%DEST%\volatile-pointer-array-discard.exe" > "%DEST%\volatile-pointer-array-discard.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /c:"would discard pointer pointee volatile qualification" "%DEST%\volatile-pointer-array-discard.log" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0VolatileVoidGlobalDiscard.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O0 --no-artifact-cache -o "%DEST%\volatile-void-global-discard.exe" > "%DEST%\volatile-void-global-discard.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /c:"would discard pointer pointee volatile qualification" "%DEST%\volatile-void-global-discard.log" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0VolatileVoidGlobalDereference.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O0 --no-artifact-cache -o "%DEST%\volatile-void-global-dereference.exe" > "%DEST%\volatile-void-global-dereference.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /c:"operand of * must be a non-void pointer" "%DEST%\volatile-void-global-dereference.log" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0VolatilePointerArrayCall.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O0 --no-artifact-cache -o "%DEST%\volatile-pointer-array-call.exe" > "%DEST%\volatile-pointer-array-call.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /c:"no viable overload of namespace function 'read_plain_int'" "%DEST%\volatile-pointer-array-call.log" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0ConstVolatilePointerArrayWrite.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O0 --no-artifact-cache -o "%DEST%\const-volatile-pointer-array-write.exe" > "%DEST%\const-volatile-pointer-array-write.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /c:"left side of assignment is not assignable" "%DEST%\const-volatile-pointer-array-write.log" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0VolatilePointerArrayAddress.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O0 --no-artifact-cache -o "%DEST%\volatile-pointer-array-address.exe" > "%DEST%\volatile-pointer-array-address.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /c:"cannot take the address of an array element with a volatile pointee" "%DEST%\volatile-pointer-array-address.log" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0FixedVolatilePointerArraySlot.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O0 --no-artifact-cache -o "%DEST%\fixed-volatile-pointer-array-slot.exe" > "%DEST%\fixed-volatile-pointer-array-slot.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /c:"left side of assignment is not assignable" "%DEST%\fixed-volatile-pointer-array-slot.log" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0VolatilePointeeDiscard.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O0 --no-artifact-cache -o "%DEST%\volatile-pointee-discard.exe" > "%DEST%\volatile-pointee-discard.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /c:"would discard pointer pointee volatile qualification" "%DEST%\volatile-pointee-discard.log" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0VolatilePointeeCall.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O0 --no-artifact-cache -o "%DEST%\volatile-pointee-call.exe" > "%DEST%\volatile-pointee-call.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /c:"no viable overload of namespace function 'read_plain_int'" "%DEST%\volatile-pointee-call.log" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0ConstVolatilePointeeWrite.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O0 --no-artifact-cache -o "%DEST%\const-volatile-pointee-write.exe" > "%DEST%\const-volatile-pointee-write.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /c:"left side of assignment is not assignable" "%DEST%\const-volatile-pointee-write.log" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0VolatileResultDiscard.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O0 --no-artifact-cache -o "%DEST%\volatile-result-discard.exe" > "%DEST%\volatile-result-discard.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /c:"would discard pointer pointee volatile qualification" "%DEST%\volatile-result-discard.log" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0VolatilePointeeTuple.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O0 --no-artifact-cache -o "%DEST%\volatile-pointee-tuple.exe" > "%DEST%\volatile-pointee-tuple.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /c:"tuple element cannot yet preserve pointer pointee volatile qualification" "%DEST%\volatile-pointee-tuple.log" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0VolatilePointeeCast.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O0 --no-artifact-cache -o "%DEST%\volatile-pointee-cast.exe" > "%DEST%\volatile-pointee-cast.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /c:"conversion would discard pointer pointee volatile qualification" "%DEST%\volatile-pointee-cast.log" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0ConstVolatilePointerWrite.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O0 --no-artifact-cache -o "%DEST%\const-volatile-pointer-write.exe" > "%DEST%\const-volatile-pointer-write.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /c:"left side of assignment is not assignable" "%DEST%\const-volatile-pointer-write.log" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0VolatilePointeeLocalAddress.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O0 --no-artifact-cache -o "%DEST%\volatile-pointee-local-address.exe" > "%DEST%\volatile-pointee-local-address.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /c:"cannot take the address of a pointer to volatile storage" "%DEST%\volatile-pointee-local-address.log" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0VolatileRecordPointeeAddress.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O0 --no-artifact-cache -o "%DEST%\volatile-record-pointee-address.exe" > "%DEST%\volatile-record-pointee-address.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /c:"cannot take the address of volatile storage" "%DEST%\volatile-record-pointee-address.log" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0VolatileAliasDiscard.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O0 --no-artifact-cache -o "%DEST%\volatile-alias-discard.exe" > "%DEST%\volatile-alias-discard.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /c:"would discard pointer pointee volatile qualification" "%DEST%\volatile-alias-discard.log" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0VolatileAliasReturnDiscard.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O0 --no-artifact-cache -o "%DEST%\volatile-alias-return-discard.exe" > "%DEST%\volatile-alias-return-discard.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /c:"return would discard pointer pointee volatile qualification" "%DEST%\volatile-alias-return-discard.log" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0ConstVolatileAliasWrite.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O0 --no-artifact-cache -o "%DEST%\const-volatile-alias-write.exe" > "%DEST%\const-volatile-alias-write.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /c:"left side of assignment is not assignable" "%DEST%\const-volatile-alias-write.log" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0ConstVolatileAliasReturnWrite.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O0 --no-artifact-cache -o "%DEST%\const-volatile-alias-return-write.exe" > "%DEST%\const-volatile-alias-return-write.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /c:"left side of assignment is not assignable" "%DEST%\const-volatile-alias-return-write.log" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0VolatileAliasGlobalAddress.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O0 --no-artifact-cache -o "%DEST%\volatile-alias-global-address.exe" > "%DEST%\volatile-alias-global-address.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /c:"cannot take the address of this global until nested pointer qualifiers are supported" "%DEST%\volatile-alias-global-address.log" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0VolatileAliasNested.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O0 --no-artifact-cache -o "%DEST%\volatile-alias-nested.exe" > "%DEST%\volatile-alias-nested.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /c:"volatile pointer alias cannot yet be represented safely" "%DEST%\volatile-alias-nested.log" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0VolatileAliasFieldDiscard.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O0 --no-artifact-cache -o "%DEST%\volatile-alias-field-discard.exe" > "%DEST%\volatile-alias-field-discard.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /c:"would discard pointer pointee volatile qualification" "%DEST%\volatile-alias-field-discard.log" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0VolatileRecordAliasCall.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O0 --no-artifact-cache -o "%DEST%\volatile-record-alias-call.exe" > "%DEST%\volatile-record-alias-call.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /c:"no viable overload of namespace function 'write_mutable_globalonly'" "%DEST%\volatile-record-alias-call.log" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0ConstVolatileRecordAliasWrite.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O0 --no-artifact-cache -o "%DEST%\const-volatile-record-alias-write.exe" > "%DEST%\const-volatile-record-alias-write.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /c:"left side of assignment is not assignable" "%DEST%\const-volatile-record-alias-write.log" >nul
if errorlevel 1 exit /b 1
echo Record C por puntero: resultado esperado cero.
exit /b 0
