@echo off
setlocal
set "TASK_ROOT=A:\vc2026-runtime"
set "TASK_TOOLSET=%TASK_ROOT%\toolsets\14.52.36815"
set "TASK_SDK=%TASK_ROOT%\windows-sdk\10.0.28000.0"
set "INCLUDE=%TASK_TOOLSET%\include;%TASK_SDK%\include\ucrt;%TASK_SDK%\include\shared;%TASK_SDK%\include\um"
set "TEMP=A:\nsc-interop-build\temp"
set "TMP=%TEMP%"
A:\al82dev-clang\bin\clang-cl.exe /nologo /std:c++20 /Zs /W4 /WX /I A:\nsc-devel\include A:\llvm-project-al82dev\nativesharp-interop\prueba-interfaces-tanda1.cpp
exit /b %ERRORLEVEL%
