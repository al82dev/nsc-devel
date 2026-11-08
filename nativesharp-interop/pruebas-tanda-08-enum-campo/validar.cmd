@echo off
setlocal
set "DEST=A:\nsc-interop-build\tanda08-enum-campo-%RANDOM%-%RANDOM%"
if exist "%DEST%" exit /b 1
mkdir "%DEST%"
if errorlevel 1 exit /b 1
set "PATH=A:\al82dev-clang\bin;A:\nsc-interop-build\tanda1-ON\bin;%PATH%"
set "NSC_LINKER=A:\al82dev-clang\bin\lld-link.exe"
set "NSC_LLVM_LIB=A:\al82dev-clang\bin\llvm-lib.exe"
set "NSC_WINDOWS_RUNTIME_LIBRARY=A:\nsc-interop-build\tanda1-ON\generated\NativeSharp.Runtime.Win32.lib"
set "TMP=%DEST%"
set "TEMP=%DEST%"
for %%F in (Api.h Main.ns Control.c Project.nsproj ProducerAnon.ns ProducerAnon.nsproj ProducerNamed.ns ProducerNamed.nsproj ConsumerNamed.ns ConsumerNamed.nsproj CppScope.h CppScope.ns CppScope.nsproj ConstReject.ns ConstReject.nsproj) do (
  copy /y "%~dp0%%F" "%DEST%\%%F" >nul
  if errorlevel 1 exit /b 1
)
clang.exe --target=x86_64-pc-windows-msvc -std=c11 -c "%DEST%\Control.c" -o "%DEST%\control.obj"
if errorlevel 1 exit /b 1
lld-link.exe /entry:main /subsystem:console /nodefaultlib /out:"%DEST%\control.exe" "%DEST%\control.obj"
if errorlevel 1 exit /b 1
"%DEST%\control.exe"
if errorlevel 1 exit /b 1
echo Referencia C de enum de campo correcta.
for %%O in (0 1 2 3) do (
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\Project.nsproj" -O%%O --no-artifact-cache -o "%DEST%\consumer-O%%O.exe" > "%DEST%\consumer-O%%O.log" 2>&1
  if errorlevel 1 goto :failure
  "%DEST%\consumer-O%%O.exe"
  if errorlevel 1 exit /b 1
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-lib "%DEST%\ProducerNamed.nsproj" -O%%O --no-artifact-cache -o "%DEST%\H4NamedProducer.lib" > "%DEST%\producer-named-O%%O.log" 2>&1
  if errorlevel 1 goto :named_failure
  nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\ConsumerNamed.nsproj" -O%%O --no-artifact-cache -o "%DEST%\named-O%%O.exe" > "%DEST%\named-O%%O.log" 2>&1
  if errorlevel 1 goto :named_failure
  "%DEST%\named-O%%O.exe"
  if errorlevel 1 exit /b 1
  echo Enum C anidado O%%O correcto.
)
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-lib "%DEST%\ProducerAnon.nsproj" -O0 --no-artifact-cache -o "%DEST%\H4AnonymousProducer.lib" > "%DEST%\producer-anon.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /c:"C enum field dependency has no importable identity" "%DEST%\producer-anon.log" >nul
if errorlevel 1 goto :anon_failure
echo Publicacion de enum anonimo rechazada sin metadata nueva.
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\CppScope.nsproj" -O0 --no-artifact-cache -o "%DEST%\cpp-scope.exe" > "%DEST%\cpp-scope.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /c:"unknown identifier 'H4CppHidden'" "%DEST%\cpp-scope.log" >nul
if errorlevel 1 goto :cpp_failure
echo Enumerador anidado C++ no se proyecta como global C.
nsc.exe --cpp-clang-root=A:\al82dev-clang --emit-project-oop-exe "%DEST%\ConstReject.nsproj" -O0 --no-artifact-cache -o "%DEST%\const-reject.exe" > "%DEST%\const-reject.log" 2>&1
if not errorlevel 1 exit /b 1
findstr /c:"error: left side of assignment is not assignable" "%DEST%\const-reject.log" >nul
if errorlevel 1 goto :const_failure
echo Escritura del enum anonimo const rechazada.
exit /b 0
:const_failure
type "%DEST%\const-reject.log"
exit /b 1
:cpp_failure
type "%DEST%\cpp-scope.log"
exit /b 1
:named_failure
type "%DEST%\producer-named-O*.log" "%DEST%\named-O*.log"
exit /b 1
:anon_failure
type "%DEST%\producer-anon.log"
exit /b 1
:failure
type "%DEST%\consumer-O*.log"
exit /b 1
