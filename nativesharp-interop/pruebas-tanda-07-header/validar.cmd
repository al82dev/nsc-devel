@echo off
setlocal
set "DEST=A:\nsc-interop-build\tanda07-header"
if not exist "%DEST%" mkdir "%DEST%"
set "TMP=%DEST%"
set "TEMP=%DEST%"
copy /y "%~dp0Project.nsproj" "%DEST%\Project.nsproj" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0Main.ns" "%DEST%\Main.ns" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0Api.h" "%DEST%\Api.h" >nul
if errorlevel 1 exit /b 1
copy /y "%~dp0Nested-before.h.txt" "%DEST%\Nested.h" >nul
if errorlevel 1 exit /b 1
"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --cpp-clang-root=A:\al82dev-clang --check-project-method-bodies "%DEST%\Project.nsproj" > "%DEST%\before.txt" 2>&1
if errorlevel 1 exit /b 1
copy /y "%~dp0Nested-after.h.txt" "%DEST%\Nested.h" >nul
if errorlevel 1 exit /b 1
"A:\nsc-interop-build\tanda1-ON\bin\nsc.exe" --cpp-clang-root=A:\al82dev-clang --check-project-method-bodies "%DEST%\Project.nsproj" > "%DEST%\after.txt" 2>&1
if not errorlevel 1 exit /b 1
findstr /i /c:"project_sum" "%DEST%\after.txt" >nul
if errorlevel 1 exit /b 1
echo La nueva firma del header transitivo se reflejo en el diagnostico de NSC.
exit /b 0
