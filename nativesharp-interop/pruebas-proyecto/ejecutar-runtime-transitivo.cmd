@echo off
setlocal
set "DEST=A:\nsc-interop-build\cpp-fixtures\runtime-transitive"
set "TOOLS=A:\al82dev-clang\bin"
if not exist "%DEST%\out" mkdir "%DEST%\out"
if exist "%DEST%\out\a.dll" del /q "%DEST%\out\a.dll"
if exist "%DEST%\out\b.dll" del /q "%DEST%\out\b.dll"
if exist "%DEST%\out\unused.dll" del /q "%DEST%\out\unused.dll"
copy /y "%~dp0CDllApi.h" "%DEST%\CDllApi.h" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0CRuntimeTransitiveUnused.h" "%DEST%\Unused.h" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0CDllMain.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0CRuntimeTransitive.nsproj" "%DEST%\Project.nsproj" >nul
if errorlevel 1 exit /b 1
set "NSC_LINKER=C:\native-sharp\bin\nslink.exe"
set "NSC_LLVM_LIB=%TOOLS%\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
for %%F in (A B Unused) do (
  "%TOOLS%\clang.exe" --target=x86_64-pc-windows-msvc -std=c11 -c "%~dp0CRuntimeTransitive%%F.c" -o "%DEST%\%%F.obj"
  if errorlevel 1 exit /b 1
)
"%TOOLS%\lld-link.exe" /nologo /dll /noentry /nodefaultlib /out:"%DEST%\b.dll" /implib:"%DEST%\b.lib" "%DEST%\B.obj"
if errorlevel 1 exit /b 1
"%TOOLS%\lld-link.exe" /nologo /dll /noentry /nodefaultlib /out:"%DEST%\unused.dll" /implib:"%DEST%\unused.lib" "%DEST%\Unused.obj"
if errorlevel 1 exit /b 1
"%TOOLS%\lld-link.exe" /nologo /dll /noentry /nodefaultlib /out:"%DEST%\a.dll" /implib:"%DEST%\a.lib" "%DEST%\A.obj" "%DEST%\b.lib"
if errorlevel 1 exit /b 1
"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-exe "%DEST%\Project.nsproj" --no-artifact-cache --cpp-clang-root=A:\al82dev-clang -o "%DEST%\out\App.exe"
if errorlevel 1 exit /b 1
if not exist "%DEST%\out\a.dll" exit /b 2
if not exist "%DEST%\out\b.dll" exit /b 3
if exist "%DEST%\out\unused.dll" exit /b 4
"%DEST%\out\App.exe"
if not errorlevel 42 exit /b 5
if errorlevel 43 exit /b 5
"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-exe "%DEST%\Project.nsproj" --no-artifact-cache --cpp-clang-root=A:\al82dev-clang -o "%DEST%\out\App.exe"
if errorlevel 1 exit /b 8
>"%DEST%\out\b.dll" echo sentinel-transitive
"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-exe "%DEST%\Project.nsproj" --no-artifact-cache --cpp-clang-root=A:\al82dev-clang -o "%DEST%\out\App.exe"
if not errorlevel 1 exit /b 6
findstr /l sentinel-transitive "%DEST%\out\b.dll" >nul
if errorlevel 1 exit /b 7
echo DLL transitiva usada empaquetada, DLL no usada omitida y colision preservada.
exit /b 0
