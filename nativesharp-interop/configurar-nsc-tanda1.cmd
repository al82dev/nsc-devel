@echo off
setlocal
if /I "%~1"=="ON" goto valid
if /I "%~1"=="OFF" goto valid
echo Uso: configurar-nsc-tanda1.cmd ON ^| OFF
exit /b 2
:valid
set "TASK_TESTS=%~2"
if "%TASK_TESTS%"=="" set "TASK_TESTS=OFF"
if /I not "%TASK_TESTS%"=="ON" if /I not "%TASK_TESTS%"=="OFF" exit /b 2
set "TASK_SYSROOT=A:\vc2026-runtime"
set "TASK_TOOLSET=%TASK_SYSROOT%\toolsets\14.52.36815"
set "TASK_SDK=%TASK_SYSROOT%\windows-sdk\10.0.28000.0"
set "INCLUDE=%TASK_TOOLSET%\include;%TASK_SDK%\include\ucrt;%TASK_SDK%\include\shared;%TASK_SDK%\include\um;%TASK_SDK%\include\winrt;%TASK_TOOLSET%\dia\include"
set "LIB=%TASK_TOOLSET%\lib\x64;%TASK_SDK%\lib\ucrt\x64;%TASK_SDK%\lib\um\x64;%TASK_TOOLSET%\dia\lib\x64"
set "TASK_BUILD=A:\nsc-interop-build\tanda1-%~1"
if not exist A:\nsc-interop-build\temp mkdir A:\nsc-interop-build\temp
set "TEMP=A:\nsc-interop-build\temp"
set "TMP=%TEMP%"
cmake -S A:/nsc-devel -B "%TASK_BUILD%" -G Ninja ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_C_COMPILER=A:/al82dev-clang/bin/clang-cl.exe ^
  -DCMAKE_CXX_COMPILER=A:/al82dev-clang/bin/clang-cl.exe ^
  -DCMAKE_LINKER=A:/al82dev-clang/bin/lld-link.exe ^
  -DCMAKE_AR=A:/al82dev-clang/bin/llvm-lib.exe ^
  -DCMAKE_RC_COMPILER=A:/al82dev-clang/bin/llvm-rc.exe ^
  -DLLVM_DIR=A:/al82dev-clang/lib/cmake/llvm ^
  -DClang_DIR=A:/al82dev-clang/lib/cmake/clang ^
  -DNSC_NSLINK_EXECUTABLE=A:/nsc-devel/build-clang/bin/nslink.exe ^
  -DNSC_HOST_LINKAGE=STATIC ^
  -DNSC_BUILD_TESTS=OFF ^
  -DNSC_ENABLE_CPP_INTEROP=%~1 ^
  -DNSC_CPP_INTEROP_SESSION_TESTS=%TASK_TESTS% ^
  -DCMAKE_PREFIX_PATH=A:/al82dev-clang-deps/install ^
  -DZLIB_INCLUDE_DIR=A:/al82dev-clang-deps/install/include ^
  -DZLIB_LIBRARY=A:/al82dev-clang-deps/install/lib/zs.lib ^
  -Dzstd_INCLUDE_DIR=A:/al82dev-clang-deps/install/include ^
  -Dzstd_LIBRARY=A:/al82dev-clang-deps/install/lib/zstd_static.lib ^
  -DLIBXML2_INCLUDE_DIR=A:/al82dev-clang-deps/install/include/libxml2 ^
  -DLIBXML2_LIBRARY=A:/al82dev-clang-deps/install/lib/libxml2s.lib ^
  -DDIASDK_INCLUDE_DIR=A:/vc2026-runtime/toolsets/14.52.36815/dia/include ^
  -DDIASDK_LIBRARIES=A:/vc2026-runtime/toolsets/14.52.36815/dia/lib/x64/diaguids.lib
exit /b %ERRORLEVEL%
