@echo off
setlocal
cd /d A:\llvm-project-al82dev\nativesharp-interop
set "TEMP=A:\llvm-project-al82dev\nativesharp-interop\temp"
set "TMP=A:\llvm-project-al82dev\nativesharp-interop\temp"
for %%G in (clang-cl.exe lld-link.exe llvm-lib.exe llvm-ml.exe llvm-rc.exe llvm-mt.exe cmake.exe ninja.exe) do (
  for %%H in ("%%~$PATH:G") do certutil -hashfile "%%~H" SHA256 > evidencia\tool-%%G-sha256.txt 2>&1
)
clang-cl --version > evidencia\bootstrap-clang-version.txt 2>&1
lld-link --version > evidencia\lld-version.txt 2>&1
certutil -hashfile A:\vc2026-runtime\manifest.sha256 SHA256 > evidencia\sysroot-root-manifest-sha256.txt 2>&1
certutil -hashfile A:\vc2026-runtime\toolsets\14.52.36815\manifest.sha256 SHA256 > evidencia\sysroot-toolset-manifest-sha256.txt 2>&1
certutil -hashfile A:\vc2026-runtime\windows-sdk\10.0.28000.0\manifest.sha256 SHA256 > evidencia\sysroot-sdk-manifest-sha256.txt 2>&1
echo Captura de herramientas terminada.
