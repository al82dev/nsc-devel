@echo off
setlocal
call "%~dp0ejecutar-runtime-transitivo.cmd"
if errorlevel 1 exit /b 1
set "SRC=A:\nsc-interop-build\cpp-fixtures\runtime-transitive"
set "DEST=A:\nsc-interop-build\cpp-fixtures\runtime-transitive-declared"
set "TOOLS=A:\al82dev-clang\bin"
if not exist "%DEST%\nested" mkdir "%DEST%\nested"
if not exist "%DEST%\out" mkdir "%DEST%\out"
if exist "%DEST%\b.dll" del /q "%DEST%\b.dll"
if exist "%DEST%\out\a.dll" del /q "%DEST%\out\a.dll"
if exist "%DEST%\out\b.dll" del /q "%DEST%\out\b.dll"
for %%F in (CDllApi.h Unused.h Main.ns a.dll a.lib) do (
  copy /y "%SRC%\%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
copy /y "%SRC%\b.dll" "%DEST%\nested\b.dll" >nul
if errorlevel 1 exit /b 1
copy /y "%SRC%\b.lib" "%DEST%\nested\b.lib" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0CRuntimeTransitiveDeclared.nsproj" "%DEST%\Project.nsproj" >nul
if errorlevel 1 exit /b 1
set "NSC_LINKER=C:\native-sharp\bin\nslink.exe"
set "NSC_LLVM_LIB=%TOOLS%\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-exe "%DEST%\Project.nsproj" --no-artifact-cache --cpp-clang-root=A:\al82dev-clang -o "%DEST%\out\App.exe"
if errorlevel 1 exit /b 1
if not exist "%DEST%\out\a.dll" exit /b 2
if not exist "%DEST%\out\b.dll" exit /b 3
"%DEST%\out\App.exe"
if not errorlevel 42 exit /b 4
if errorlevel 43 exit /b 4
echo Runtime transitivo declarado en otra carpeta empaquetado y ejecutado: 42.
exit /b 0
