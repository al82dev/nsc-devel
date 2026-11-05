@echo off
setlocal
set "DEST=A:\nsc-interop-build\cpp-fixtures\importacion-coff-larga"
set "TOOLS=A:\al82dev-clang\bin"
if not exist "%DEST%" mkdir "%DEST%"
if not exist "%DEST%\temp" mkdir "%DEST%\temp"
set "TMP=%DEST%\temp"
set "TEMP=%DEST%\temp"
"%TOOLS%\llvm-mc.exe" -triple=x86_64-pc-windows-msvc -filetype=obj "%~dp0long-import.s" -o "%DEST%\long-import.obj"
if errorlevel 1 exit /b 2
if exist "%DEST%\long.lib" del /q "%DEST%\long.lib"
"%TOOLS%\llvm-ar.exe" rcs "%DEST%\long.lib" "%DEST%\long-import.obj"
if errorlevel 1 exit /b 3
"%TOOLS%\llvm-dlltool.exe" -I "%DEST%\long.lib" > "%DEST%\identify.log" 2>&1
if errorlevel 1 exit /b 4
findstr /l /c:"foo.dll" "%DEST%\identify.log" >nul
if errorlevel 1 exit /b 5
"%TOOLS%\clang.exe" --target=x86_64-pc-windows-msvc -c "%~dp0foo.c" -o "%DEST%\foo.obj"
if errorlevel 1 exit /b 6
"%TOOLS%\lld-link.exe" /nologo /dll /noentry /nodefaultlib /out:"%DEST%\foo.dll" /implib:"%DEST%\short.lib" "%DEST%\foo.obj"
if errorlevel 1 exit /b 7
"%TOOLS%\clang.exe" --target=x86_64-pc-windows-msvc -c "%~dp0main.c" -o "%DEST%\main.obj"
if errorlevel 1 exit /b 8
"%TOOLS%\lld-link.exe" /nologo /nodefaultlib /opt:noref /entry:main /subsystem:console /out:"%DEST%\native.exe" "%DEST%\main.obj" "%DEST%\long.lib"
if errorlevel 1 exit /b 9
"%DEST%\native.exe"
if not errorlevel 42 exit /b 10
if errorlevel 43 exit /b 10
copy /y "%~dp0Api.h" "%DEST%\Api.h" >nul
if errorlevel 1 exit /b 11
copy /y "%~dp0Main.ns.txt" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 11
copy /y "%~dp0Project.nsproj" "%DEST%\Project.nsproj" >nul
if errorlevel 1 exit /b 11
copy /y "%~dp0Mixed.nsproj" "%DEST%\Mixed.nsproj" >nul
if errorlevel 1 exit /b 11
"%TOOLS%\llvm-lib.exe" /out:"%DEST%\mixed.lib" "%DEST%\long.lib" "%DEST%\short.lib"
if errorlevel 1 exit /b 17
set "NSC_LINKER=C:\native-sharp\bin\nslink.exe"
set "NSC_LLVM_LIB=%TOOLS%\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
python "%~dp0verificar-largo-malformado.py" "%DEST%"
if errorlevel 1 exit /b 16
if exist "%DEST%\ns.exe" del /q "%DEST%\ns.exe"
if exist "%DEST%\ns.exe" exit /b 12
"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-exe "%DEST%\Project.nsproj" -O0 --no-artifact-cache --cpp-clang-root=A:\al82dev-clang -o "%DEST%\ns.exe" > "%DEST%\nsc.log" 2>&1
if not errorlevel 1 exit /b 13
findstr /l /c:"COFF long import thunk requires native linking support" "%DEST%\nsc.log" >nul
if errorlevel 1 exit /b 14
if exist "%DEST%\ns.exe" exit /b 15
"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --emit-project-oop-exe "%DEST%\Mixed.nsproj" -O0 --no-artifact-cache --cpp-clang-root=A:\al82dev-clang -o "%DEST%\mixed.exe" > "%DEST%\mixed.log" 2>&1
if not errorlevel 1 exit /b 18
findstr /l /c:"COFF long import thunk requires native linking support" "%DEST%\mixed.log" >nul
if errorlevel 1 exit /b 19
echo Importacion larga ejecutada con LLD y rechazada por NSC hasta resolver el enlace mixto.
exit /b 0
