@echo off
setlocal
cd /d A:\llvm-project-al82dev\nativesharp-interop
if not exist evidencia mkdir evidencia
if not exist temp mkdir temp
set "TEMP=A:\llvm-project-al82dev\nativesharp-interop\temp"
set "TMP=A:\llvm-project-al82dev\nativesharp-interop\temp"
git -C A:\llvm-project-al82dev rev-parse HEAD > evidencia\llvm-commit.txt 2>&1
git -C A:\llvm-project-al82dev status --short > evidencia\llvm-status.txt 2>&1
git -C A:\nsc-devel rev-parse HEAD > evidencia\nsc-commit.txt 2>&1
nsc --version > evidencia\nsc-version.txt 2>&1
for %%G in (nsc.exe) do set "NSC_EXE=%%~$PATH:G"
if not defined NSC_EXE exit /b 2
certutil -hashfile "%NSC_EXE%" SHA256 > evidencia\nsc-sha256.txt 2>&1
llvm-readobj --coff-imports "%NSC_EXE%" > evidencia\nsc-imports.txt 2>&1
A:\al82dev-clang\bin\clang-cl.exe --version > evidencia\clang-version.txt 2>&1
if errorlevel 1 exit /b 1
certutil -hashfile A:\al82dev-clang\bin\clang.exe SHA256 > evidencia\clang-sha256.txt 2>&1
llvm-readobj --coff-imports A:\al82dev-clang\bin\clang.exe > evidencia\clang-imports.txt 2>&1
ninja -C A:\al82dev-clang -t commands clang > evidencia\clang-commands.txt 2>&1
if errorlevel 1 exit /b 1
cmake --version > evidencia\cmake-version.txt 2>&1
ninja --version > evidencia\ninja-version.txt 2>&1
echo Baseline evidence captured.
