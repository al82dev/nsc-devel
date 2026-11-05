@echo off
setlocal
set "DEST=A:\nsc-interop-build\cpp-fixtures\importacion-coff-larga"
set "TOOLS=A:\al82dev-clang\bin"
if not exist "%DEST%\long.lib" exit /b 1
"%TOOLS%\llvm-dlltool.exe" -m i386:x86-64 -d "%~dp0kernel32.def" -l "%DEST%\kernel32-test.lib"
if errorlevel 1 exit /b 2
"%TOOLS%\clang.exe" --target=x86_64-pc-windows-msvc -c "%~dp0mixed-main.c" -o "%DEST%\mixed-main.obj"
if errorlevel 1 exit /b 3
"%TOOLS%\lld-link.exe" /nologo /nodefaultlib /entry:entry /subsystem:console /out:"%DEST%\mixed-lld.exe" "%DEST%\mixed-main.obj" "%DEST%\long.lib" "%DEST%\kernel32-test.lib"
if errorlevel 1 exit /b 4
"%TOOLS%\llvm-readobj.exe" --coff-imports "%DEST%\mixed-lld.exe" > "%DEST%\mixed-lld-imports.log"
if errorlevel 1 exit /b 5
"%DEST%\mixed-lld.exe"
set "LLD_RESULT=%errorlevel%"
echo LLD result: %LLD_RESULT%
findstr /i /c:"Name: foo.dll" "%DEST%\mixed-lld-imports.log" >nul
if errorlevel 1 echo LLD import table does not list foo.dll.
nslink /nologo /nodefaultlib /entry:entry /subsystem:console /out:"%DEST%\mixed-nslink.exe" "%DEST%\mixed-main.obj" "%DEST%\long.lib" "%DEST%\kernel32-test.lib"
if errorlevel 1 exit /b 6
"%TOOLS%\llvm-readobj.exe" --coff-imports "%DEST%\mixed-nslink.exe" > "%DEST%\mixed-nslink-imports.log"
if errorlevel 1 exit /b 7
findstr /i /c:"Name: foo.dll" "%DEST%\mixed-nslink-imports.log" >nul
if not errorlevel 1 exit /b 8
findstr /i /c:"Name: KERNEL32.dll" "%DEST%\mixed-nslink-imports.log" >nul
if errorlevel 1 exit /b 9
if not "%LLD_RESULT%"=="42" exit /b 10
echo LLD incluye foo.dll y NSLink no; se conserva la barrera de NSC.
exit /b 0
