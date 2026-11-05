@echo off
setlocal
set "DEST=A:\nsc-interop-build\cpp-fixtures\command-cpp-local-dll"
set "TOOLS=A:\al82dev-clang\bin"
if not exist "%DEST%\temp" mkdir "%DEST%\temp"
set "TMP=%DEST%\temp"
set "TEMP=%DEST%\temp"
for %%F in (Ready.h Ready.cpp Project.nsproj) do copy /y "%~dp0%%F" "%DEST%\%%F" >nul
set "NSC_LINKER=C:\native-sharp\bin\nslink.exe"
set "NSC_LLVM_LIB=%TOOLS%\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
"%TOOLS%\clang.exe" --target=x86_64-pc-windows-msvc -std=c++20 -DBUILDING_API -c "%DEST%\Ready.cpp" -o "%DEST%\ready.obj"
if errorlevel 1 goto failure
"%TOOLS%\lld-link.exe" /nologo /dll /noentry /nodefaultlib /out:"%DEST%\ready.dll" /implib:"%DEST%\ready.lib" "%DEST%\ready.obj"
if errorlevel 1 goto failure
for %%S in (Scope Return Exception Break Continue) do for %%O in (0 2) do (
  copy /y "%~dp0..\lifetime-local\%%S.ns.txt" "%DEST%\Main.ns" >nul
  "A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-exe "%DEST%\Project.nsproj" -O%%O --no-artifact-cache --cpp-clang-root=A:\al82dev-clang -o "%DEST%\%%S-O%%O.exe" > "%DEST%\%%S-O%%O.log" 2>&1
  if errorlevel 1 goto failure
  "%TOOLS%\llvm-readobj.exe" --coff-imports "%DEST%\%%S-O%%O.exe" > "%DEST%\%%S-O%%O-imports.txt"
  if errorlevel 1 goto failure
  findstr /l /c:"Name: ready.dll" "%DEST%\%%S-O%%O-imports.txt" >nul
  if errorlevel 1 goto failure
  findstr /l /c:"??0Ready@Demo@@QEAA@H@Z" "%DEST%\%%S-O%%O-imports.txt" >nul
  if errorlevel 1 goto failure
  findstr /l /c:"??1Ready@Demo@@QEAA@XZ" "%DEST%\%%S-O%%O-imports.txt" >nul
  if errorlevel 1 goto failure
  "%DEST%\%%S-O%%O.exe"
  if errorlevel 68 goto failure
  if not errorlevel 67 goto failure
)
echo Locales C++ noexcept desde DLL: ambito, retorno, excepcion, break y continue correctos en O0/O2.
exit /b 0
:failure
echo Fallo la prueba de locales C++ en DLL; revisar registros en %DEST%.
exit /b 1
