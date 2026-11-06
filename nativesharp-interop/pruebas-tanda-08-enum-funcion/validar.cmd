@echo off
setlocal
set "DEST=A:\nsc-interop-build\tanda08-enum-funcion"
set "PATH=A:\al82dev-clang\bin;A:\nsc-interop-build\tanda1-ON\bin;%PATH%"
if not exist "%DEST%" mkdir "%DEST%"
if errorlevel 1 exit /b 1
set "TMP=%DEST%"
set "TEMP=%DEST%"
set "NSC_LINKER=A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
for %%F in (Project.nsproj Main.ns Api.h Api.c StoredCallback.ns StoredCallback.nsproj) do (
  copy /y "%~dp0%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 "%DEST%\Api.c" -o "%DEST%\api.obj"
if errorlevel 1 exit /b 1
llvm-lib.exe /nologo /out:"%DEST%\api.lib" "%DEST%\api.obj"
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --dump-project-oop-llvm "%DEST%\Project.nsproj" > "%DEST%\project.ll"
if errorlevel 1 exit /b 1
findstr /r /c:"load volatile.*@volatile_mood" "%DEST%\project.ll" >nul
if errorlevel 1 (echo Falta lectura LLVM volatile del enum. & exit /b 1)
findstr /r /c:"store volatile.*@volatile_mood" "%DEST%\project.ll" >nul
if errorlevel 1 (echo Falta escritura LLVM volatile del enum. & exit /b 1)
findstr /r /c:"load volatile i32, ptr" "%DEST%\project.ll" >nul
if errorlevel 1 (echo Falta lectura volatile de enum. & exit /b 1)
findstr /r /c:"load volatile.*@volatile_states" "%DEST%\project.ll" >nul
if errorlevel 1 (echo Falta lectura LLVM volatile del arreglo enum. & exit /b 1)
findstr /r /c:"store volatile.*@volatile_states" "%DEST%\project.ll" >nul
if errorlevel 1 (echo Falta escritura LLVM volatile del arreglo enum. & exit /b 1)
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O0 --no-artifact-cache -o "%DEST%\program.exe"
if errorlevel 1 exit /b 1
"%DEST%\program.exe"
if errorlevel 1 exit /b 1
for %%O in (0 1 2 3) do (
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\StoredCallback.nsproj" -O%%O --no-artifact-cache -o "%DEST%\stored-callback-O%%O.exe"
  if errorlevel 1 exit /b 1
  "%DEST%\stored-callback-O%%O.exe"
  if errorlevel 1 exit /b 1
  echo Callback almacenado con enum C O%%O: resultado esperado cero.
)
copy /y "%~dp0Shadow.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O0 --no-artifact-cache -o "%DEST%\shadow.exe"
if errorlevel 1 exit /b 1
"%DEST%\shadow.exe"
if errorlevel 1 exit /b 1
copy /y "%~dp0Collision.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O0 --no-artifact-cache -o "%DEST%\collision.exe" > "%DEST%\collision.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /i /c:"C/C++ global type is not yet mapped" "%DEST%\collision.log" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0ConstWrite.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O0 --no-artifact-cache -o "%DEST%\const-write.exe" > "%DEST%\const-write.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /i /c:"readonly_mood" "%DEST%\const-write.log" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0ConstArrayWrite.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O0 --no-artifact-cache -o "%DEST%\const-array-write.exe" > "%DEST%\const-array-write.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /i /c:"not assignable" "%DEST%\const-array-write.log" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0ConstVolatileParameter.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O0 --no-artifact-cache -o "%DEST%\const-volatile-parameter.exe" > "%DEST%\const-volatile-parameter.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /c:"no viable overload of namespace function 'update_volatile_mood'" "%DEST%\const-volatile-parameter.log" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0VolatileEnumAliasCall.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O0 --no-artifact-cache -o "%DEST%\volatile-enum-alias-call.exe" > "%DEST%\volatile-enum-alias-call.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /c:"no viable overload of namespace function 'read_plain_mood'" "%DEST%\volatile-enum-alias-call.log" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0ConstVolatileEnumAliasWrite.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O0 --no-artifact-cache -o "%DEST%\const-volatile-enum-alias-write.exe" > "%DEST%\const-volatile-enum-alias-write.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /c:"left side of assignment is not assignable" "%DEST%\const-volatile-enum-alias-write.log" >nul
if errorlevel 1 exit /b 1
echo Enum C en funciones: resultado esperado cero.
exit /b 0
