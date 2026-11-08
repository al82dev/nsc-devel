@echo off
setlocal EnableDelayedExpansion
set "DEST=A:\nsc-interop-build\tanda08-nombres-metadata-referenciados"
set "PATH=A:\al82dev-clang\bin;A:\nsc-interop-build\tanda1-ON\bin;%PATH%"
set "NSC_LINKER=A:\nsc-interop-build\cpp-fixtures\command-cpp\nslink.exe"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
if not exist "%DEST%" mkdir "%DEST%"
if errorlevel 1 exit /b 1
set "TEMP=%DEST%"
set "TMP=%DEST%"
for %%F in (Api.h Api.c ApiConflict.h ApiConflictEnum.h ProducerReferenced.ns ProducerReferenced.nsproj ConsumerReferenced.ns ConsumerReferenced.nsproj ConsumerEnumConflict.ns ConflictEnum.nsproj SharedReferenced.nsproj ConflictReferenced.nsproj) do (
  copy /y "%~dp0%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
clang.exe -c --target=x86_64-pc-windows-msvc -std=c11 "%DEST%\Api.c" -o "%DEST%\api.obj"
if errorlevel 1 exit /b 1
llvm-lib.exe /nologo /out:"%DEST%\api.lib" "%DEST%\api.obj"
if errorlevel 1 exit /b 1
for %%O in (0 1 2 3) do (
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-lib "%DEST%\ProducerReferenced.nsproj" -O%%O --no-artifact-cache -o "%DEST%\CNamesReferenced.lib"
  if errorlevel 1 exit /b 1
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\SharedReferenced.nsproj" -O%%O --no-artifact-cache -o "%DEST%\Shared-O%%O.exe"
  if errorlevel 1 exit /b 1
  "%DEST%\Shared-O%%O.exe"
  if errorlevel 1 exit /b 1
  move /y "%DEST%\Api.h" "%DEST%\Api.hidden" >nul
  if errorlevel 1 exit /b 1
  call :consumer %%O
  set "RESULT=!ERRORLEVEL!"
  move /y "%DEST%\Api.hidden" "%DEST%\Api.h" >nul
  if errorlevel 1 exit /b 1
  if not "!RESULT!"=="0" exit /b !RESULT!
)
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\ConflictReferenced.nsproj" -O0 --no-artifact-cache -o "%DEST%\Conflict.exe" > "%DEST%\Conflict.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /c:"conflicting C/C++ type alias" "%DEST%\Conflict.log" >nul
if errorlevel 1 exit /b 1
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\ConflictEnum.nsproj" -O0 --no-artifact-cache -o "%DEST%\ConflictEnum.exe" > "%DEST%\ConflictEnum.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /c:"NMood" "%DEST%\ConflictEnum.log" >nul
if errorlevel 1 exit /b 1
exit /b 0

:consumer
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\ConsumerReferenced.nsproj" -O%1 --no-artifact-cache -o "%DEST%\Consumer-O%1.exe"
if errorlevel 1 exit /b 1
"%DEST%\Consumer-O%1.exe"
exit /b %ERRORLEVEL%
