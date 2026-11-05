@echo off
setlocal
set "DEST=A:\nsc-interop-build\cpp-fixtures\cpp-local-reference"
set "TOOLS=A:\al82dev-clang\bin"
if not exist "%DEST%\temp" mkdir "%DEST%\temp"
set "TMP=%DEST%\temp"
set "TEMP=%DEST%\temp"
for %%F in (CppClassApi.h CppClassApi.cpp) do (
  copy /y "%~dp0..\%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
copy /y "%~dp0Main.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0Project.nsproj" "%DEST%\Project.nsproj" >nul
if errorlevel 1 exit /b 1
set "NSC_LINKER=C:\native-sharp\bin\nslink.exe"
set "NSC_LLVM_LIB=%TOOLS%\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
"%TOOLS%\clang.exe" --target=x86_64-pc-windows-msvc -std=c++20 -c "%DEST%\CppClassApi.cpp" -o "%DEST%\class-api.obj"
if errorlevel 1 exit /b 2
"%TOOLS%\llvm-lib.exe" /nologo /out:"%DEST%\class-api.lib" "%DEST%\class-api.obj"
if errorlevel 1 exit /b 3
for %%O in (0 2) do (
  if exist "%DEST%\test-O%%O.exe" del /q "%DEST%\test-O%%O.exe"
  if exist "%DEST%\test-O%%O.exe" exit /b 14
  "A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-exe "%DEST%\Project.nsproj" -O%%O --no-artifact-cache --cpp-clang-root=A:\al82dev-clang -o "%DEST%\test-O%%O.exe"
  if errorlevel 1 exit /b 4
  if not exist "%DEST%\test-O%%O.exe" exit /b 15
  "%DEST%\test-O%%O.exe"
  if not errorlevel 26 exit /b 5
  if errorlevel 27 exit /b 5
)
copy /y "%~dp0OwnedApi.h" "%DEST%\OwnedApi.h" >nul
if errorlevel 1 exit /b 9
copy /y "%~dp0OwnedApi.cpp" "%DEST%\OwnedApi.cpp" >nul
if errorlevel 1 exit /b 9
copy /y "%~dp0Borrowed.ns.txt" "%DEST%\Borrowed.ns" >nul
if errorlevel 1 exit /b 9
copy /y "%~dp0Borrowed.nsproj" "%DEST%\Borrowed.nsproj" >nul
if errorlevel 1 exit /b 9
"%TOOLS%\clang.exe" --target=x86_64-pc-windows-msvc -std=c++20 -fno-exceptions -c "%DEST%\OwnedApi.cpp" -o "%DEST%\owned-api.obj"
if errorlevel 1 exit /b 10
"%TOOLS%\llvm-lib.exe" /nologo /out:"%DEST%\owned-api.lib" "%DEST%\owned-api.obj"
if errorlevel 1 exit /b 11
for %%O in (0 2) do (
  if exist "%DEST%\borrowed-O%%O.exe" del /q "%DEST%\borrowed-O%%O.exe"
  if exist "%DEST%\borrowed-O%%O.exe" exit /b 16
  "A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-exe "%DEST%\Borrowed.nsproj" -O%%O --no-artifact-cache --cpp-clang-root=A:\al82dev-clang -o "%DEST%\borrowed-O%%O.exe"
  if errorlevel 1 exit /b 12
  if not exist "%DEST%\borrowed-O%%O.exe" exit /b 17
  "%DEST%\borrowed-O%%O.exe"
  if not errorlevel 26 exit /b 13
  if errorlevel 27 exit /b 13
)
copy /y "%~dp0ValueLocal.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 6
"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-exe "%DEST%\Project.nsproj" -O0 --no-artifact-cache --cpp-clang-root=A:\al82dev-clang -o "%DEST%\value-local.exe" > "%DEST%\value-local.log" 2>&1
if errorlevel 1 exit /b 7
"%DEST%\value-local.exe"
if errorlevel 1 exit /b 8
echo Referencia local C++ y puntero original observan el mismo objeto: 26; local trivial admitido.
exit /b 0
