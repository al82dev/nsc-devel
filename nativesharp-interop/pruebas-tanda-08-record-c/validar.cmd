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
llvm-lib.exe /nologo /out:"%DEST%\api.lib" "%DEST%\api.obj"
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --dump-project-oop-llvm "%DEST%\Project.nsproj" > "%DEST%\project.ll"
if errorlevel 1 exit /b 1
findstr /r /c:"load volatile.*%%field.addr" "%DEST%\project.ll" >nul
if errorlevel 1 (echo Falta lectura LLVM volatile del campo. & exit /b 1)
findstr /r /c:"store volatile.*%%field.addr" "%DEST%\project.ll" >nul
if errorlevel 1 (echo Falta escritura LLVM volatile del campo. & exit /b 1)
findstr /r /c:"load volatile.*@shared_volatile" "%DEST%\project.ll" >nul
if errorlevel 1 (echo Falta lectura LLVM volatile del global. & exit /b 1)
findstr /r /c:"store volatile.*@shared_volatile" "%DEST%\project.ll" >nul
if errorlevel 1 (echo Falta escritura LLVM volatile del global. & exit /b 1)
findstr /r /c:"load volatile.*@volatile_array" "%DEST%\project.ll" >nul
if errorlevel 1 (echo Falta lectura LLVM volatile del arreglo. & exit /b 1)
findstr /r /c:"store volatile.*@volatile_array" "%DEST%\project.ll" >nul
if errorlevel 1 (echo Falta escritura LLVM volatile del arreglo. & exit /b 1)
findstr /r /c:"load volatile.*@volatile_matrix" "%DEST%\project.ll" >nul
if errorlevel 1 (echo Falta lectura LLVM volatile de la matriz. & exit /b 1)
findstr /r /c:"store volatile.*@volatile_matrix" "%DEST%\project.ll" >nul
if errorlevel 1 (echo Falta escritura LLVM volatile de la matriz. & exit /b 1)
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
findstr /r /c:"load volatile ptr, ptr @volatile_record_pointer" "%DEST%\project.ll" >nul
if errorlevel 1 (echo Falta lectura volatile del puntero. & exit /b 1)
findstr /r /c:"store i32 17, ptr" "%DEST%\project.ll" >nul
if errorlevel 1 (echo Falta escritura ordinaria en el objeto apuntado. & exit /b 1)
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O0 --no-artifact-cache -o "%DEST%\program.exe"
if errorlevel 1 exit /b 1
"%DEST%\program.exe"
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
findstr /i /c:"const" /c:"readonly" /c:"mutability" "%DEST%\const-record-pointer-conversion.log" >nul
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
echo Record C por puntero: resultado esperado cero.
exit /b 0
