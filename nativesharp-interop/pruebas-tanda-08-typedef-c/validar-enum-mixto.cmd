@echo off
setlocal
set "DEST=A:\nsc-interop-build\tanda08-typedef-c"
set "PATH=A:\al82dev-clang\bin;A:\nsc-interop-build\tanda1-ON\bin;%PATH%"
set "TMP=%DEST%"
set "TEMP=%DEST%"
set "NSC_LINKER=A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
for %%O in (0 1 2 3) do (
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\EnumMixed.nsproj" -O%%O --no-artifact-cache -o "%DEST%\enum-mixed-O%%O.exe"
  if errorlevel 1 exit /b 1
  "%DEST%\enum-mixed-O%%O.exe"
  if errorlevel 1 exit /b 1
  echo Enum C mixto O%%O coincide con C.
)
echo ENUM MIXTO C COMPLETO
exit /b 0
