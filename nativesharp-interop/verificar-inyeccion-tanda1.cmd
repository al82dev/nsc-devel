@echo off
setlocal
set "TASK_ROOT=A:\vc2026-runtime"
set "TASK_TOOLSET=%TASK_ROOT%\toolsets\14.52.36815"
set "TASK_SDK=%TASK_ROOT%\windows-sdk\10.0.28000.0"
set "INCLUDE=%TASK_TOOLSET%\include;%TASK_SDK%\include\ucrt;%TASK_SDK%\include\shared;%TASK_SDK%\include\um;%TASK_SDK%\include\winrt"
set "LIB=%TASK_TOOLSET%\lib\x64;%TASK_SDK%\lib\ucrt\x64;%TASK_SDK%\lib\um\x64"
set "PATH=A:\al82dev-clang\bin;%PATH%"
set "TEMP=A:\nsc-interop-build\temp"
set "TMP=%TEMP%"
cd /d A:\nsc-interop-build
A:\al82dev-clang\bin\clang-cl.exe /nologo /std:c++20 /MT /W4 /WX /clang:-fuse-ld=lld /I A:\nsc-devel\include /I A:\nsc-interop-build\tanda1-OFF\generated /FeA:\nsc-interop-build\prueba-inyeccion.exe A:\llvm-project-al82dev\nativesharp-interop\prueba-inyeccion-tanda1.cpp A:\nsc-interop-build\tanda1-OFF\lib\nsc_frontend.lib A:\nsc-interop-build\tanda1-OFF\lib\nsc_precompiled.lib A:\nsc-interop-build\tanda1-OFF\lib\nsc_compilation.lib A:\nsc-interop-build\tanda1-OFF\lib\nsc_target.lib A:\nsc-interop-build\tanda1-OFF\lib\nsc_basic.lib /link /nologo
if errorlevel 1 exit /b %ERRORLEVEL%
A:\nsc-interop-build\prueba-inyeccion.exe
exit /b %ERRORLEVEL%
