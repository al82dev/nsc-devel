@echo off
setlocal
set "DEST=A:\nsc-interop-build\tanda08-typedef-c"
set "PATH=A:\al82dev-clang\bin;A:\nsc-interop-build\tanda1-ON\bin;%PATH%"
if not exist "%DEST%" mkdir "%DEST%"
if errorlevel 1 exit /b 1
set "TMP=%DEST%"
set "TEMP=%DEST%"
set "NSC_LINKER=A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
for %%F in (Api.h Api.c Main.ns Project.nsproj MatrixPointer.ns MatrixPointer.nsproj InvalidConstMatrix.ns InvalidConstMatrix.nsproj InvalidMatrixSize.ns InvalidMatrixSize.nsproj PointArrayPointer.ns PointArrayPointer.nsproj ColorArrayPointer.ns ColorArrayPointer.nsproj InvalidConstPoints.ns InvalidConstPoints.nsproj InvalidConstColors.ns InvalidConstColors.nsproj InvalidArrayIdentity.ns InvalidArrayIdentity.nsproj Enumerator.ns Enumerator.nsproj InvalidNativeEnum.ns InvalidNativeEnum.nsproj) do (
  copy /y "%~dp0%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
for %%F in (EnumToInteger InvalidNativeEnumToInteger EnumArithmetic InvalidNativeEnumArithmetic NativeEnumBitwise EnumMixed) do (
  copy /y "%~dp0%%F.ns" "%DEST%\%%F.ns" >nul
  if errorlevel 1 exit /b 1
  copy /y "%~dp0%%F.nsproj" "%DEST%\%%F.nsproj" >nul
  if errorlevel 1 exit /b 1
)
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 "%DEST%\Api.c" -o "%DEST%\api.obj"
if errorlevel 1 exit /b 1
llvm-lib.exe /nologo /out:"%DEST%\api.lib" "%DEST%\api.obj"
if errorlevel 1 exit /b 1
for %%O in (0 1 2 3) do (
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O%%O --no-artifact-cache -o "%DEST%\consumer-O%%O.exe"
  if errorlevel 1 exit /b 1
  "%DEST%\consumer-O%%O.exe"
  if errorlevel 1 exit /b 1
  echo Typedef C real O%%O ejecutado correctamente.
)
echo MATRIZ TYPEDEF C COMPLETA
exit /b 0
