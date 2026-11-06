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
findstr /c:"C/C++ global type is not yet mapped" "%DEST%\const-record.log" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0VolatileRecordGlobal.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O0 --no-artifact-cache -o "%DEST%\volatile-record.exe" > "%DEST%\volatile-record.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /c:"C/C++ global type is not yet mapped" "%DEST%\volatile-record.log" >nul
if errorlevel 1 exit /b 1
echo Record C por puntero: resultado esperado cero.
exit /b 0
