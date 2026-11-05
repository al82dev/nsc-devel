@echo off
setlocal
set "DEST=A:\nsc-interop-build\cpp-fixtures\command-cpp-scalar-dll"
set "TOOLS=A:\al82dev-clang\bin"
if not exist "%DEST%\temp" mkdir "%DEST%\temp"
set "TMP=%DEST%\temp"
set "TEMP=%DEST%\temp"
for %%F in (Scalar.h Project.nsproj) do copy /y "%~dp0%%F" "%DEST%\%%F" >nul
copy /y "%~dp0..\lifetime-scalar\Scalar.cpp" "%DEST%\Scalar.cpp" >nul
set "NSC_LINKER=C:\native-sharp\bin\nslink.exe"
set "NSC_LLVM_LIB=%TOOLS%\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
"%TOOLS%\clang.exe" --target=x86_64-pc-windows-msvc -std=c++20 -DBUILDING_API -c "%DEST%\Scalar.cpp" -o "%DEST%\scalar.obj"
if errorlevel 1 goto failure
"%TOOLS%\lld-link.exe" /nologo /dll /noentry /nodefaultlib /out:"%DEST%\scalar.dll" /implib:"%DEST%\scalar.lib" "%DEST%\scalar.obj" "A:\vc2026-runtime\lib\x64\release\libcmt.lib"
if errorlevel 1 goto failure
for %%S in (Main Wide Narrow Pointer ReadPointer Readonly ChoiceMutable ChoiceReadonly) do for %%O in (0 2) do (
  copy /y "%~dp0..\lifetime-scalar\%%S.ns.txt" "%DEST%\Main.ns" >nul
  "A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-exe "%DEST%\Project.nsproj" -O%%O --no-artifact-cache --cpp-clang-root=A:\al82dev-clang -o "%DEST%\%%S-O%%O.exe" > "%DEST%\%%S-O%%O.log" 2>&1
  if errorlevel 1 goto failure
  "%TOOLS%\llvm-readobj.exe" --coff-imports "%DEST%\%%S-O%%O.exe" > "%DEST%\%%S-O%%O-imports.txt"
  if errorlevel 1 goto failure
  findstr /l /c:"Name: scalar.dll" "%DEST%\%%S-O%%O-imports.txt" >nul
  if errorlevel 1 goto failure
  if "%%S"=="Main" findstr /l /c:"??0Scalar@Demo@@" "%DEST%\%%S-O%%O-imports.txt" >nul
  if "%%S"=="Wide" findstr /l /c:"??0Wide@Demo@@" "%DEST%\%%S-O%%O-imports.txt" >nul
  if "%%S"=="Narrow" findstr /l /c:"??0Narrow@Demo@@" "%DEST%\%%S-O%%O-imports.txt" >nul
  if "%%S"=="Pointer" findstr /l /c:"??0Pointer@Demo@@" "%DEST%\%%S-O%%O-imports.txt" >nul
  if "%%S"=="ReadPointer" findstr /l /c:"??0ReadPointer@Demo@@" "%DEST%\%%S-O%%O-imports.txt" >nul
  if "%%S"=="Readonly" findstr /l /c:"??0ReadPointer@Demo@@" "%DEST%\%%S-O%%O-imports.txt" >nul
  if "%%S"=="ChoiceMutable" findstr /l /c:"??0Choice@Demo@@QEAA@PEAH@Z" "%DEST%\%%S-O%%O-imports.txt" >nul
  if "%%S"=="ChoiceReadonly" findstr /l /c:"??0Choice@Demo@@QEAA@PEBH@Z" "%DEST%\%%S-O%%O-imports.txt" >nul
  if errorlevel 1 goto failure
  if "%%S"=="Main" findstr /l /c:"??1Scalar@Demo@@" "%DEST%\%%S-O%%O-imports.txt" >nul
  if "%%S"=="Wide" findstr /l /c:"??1Wide@Demo@@" "%DEST%\%%S-O%%O-imports.txt" >nul
  if "%%S"=="Narrow" findstr /l /c:"??1Narrow@Demo@@" "%DEST%\%%S-O%%O-imports.txt" >nul
  if "%%S"=="Pointer" findstr /l /c:"??1Pointer@Demo@@" "%DEST%\%%S-O%%O-imports.txt" >nul
  if "%%S"=="ReadPointer" findstr /l /c:"??1ReadPointer@Demo@@" "%DEST%\%%S-O%%O-imports.txt" >nul
  if "%%S"=="Readonly" findstr /l /c:"??1ReadPointer@Demo@@" "%DEST%\%%S-O%%O-imports.txt" >nul
  if "%%S"=="ChoiceMutable" findstr /l /c:"??1Choice@Demo@@" "%DEST%\%%S-O%%O-imports.txt" >nul
  if "%%S"=="ChoiceReadonly" findstr /l /c:"??1Choice@Demo@@" "%DEST%\%%S-O%%O-imports.txt" >nul
  if errorlevel 1 goto failure
  "%DEST%\%%S-O%%O.exe"
  if errorlevel 68 goto failure
  if not errorlevel 67 goto failure
)
echo Constructores C++ escalares desde DLL y destructores correctos en O0/O2.
exit /b 0
:failure
echo Fallo la prueba de constructores C++ escalares en DLL; revisar registros en %DEST%.
exit /b 1
