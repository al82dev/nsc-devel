@echo off
setlocal EnableExtensions DisableDelayedExpansion

if "%~5"=="" goto :usage

for %%I in ("%~1") do set "SH44_SOURCE=%%~fI"
for %%I in ("%~2") do set "SH44_SH41=%%~fI"
for %%I in ("%~3") do set "SH44_OUTPUT=%%~fI"
for %%I in ("%~4") do set "SH44_PREFIX=%%~fI"
for %%I in ("%~5") do set "SH44_LLVM=%%~fI"
set "SH44_PEER=%~6"
set "SH44_RUNTIME=%SH44_PREFIX%\lib\nativesharp\runtime\NativeSharp.Runtime.Win32.lib"
if exist "%SH44_SOURCE%\runtime\NativeSharp.Runtime.Win32.lib" set "SH44_RUNTIME=%SH44_SOURCE%\runtime\NativeSharp.Runtime.Win32.lib"

where python.exe >nul 2>nul || (echo SH44: python.exe is not available in PATH.& exit /b 2)
where cmake.exe >nul 2>nul || (echo SH44: cmake.exe is not available in PATH.& exit /b 2)
for /f "delims=" %%I in ('where python.exe') do if not defined SH44_PYTHON set "SH44_PYTHON=%%I"

if not exist "%SH44_SOURCE%\cmake\VerifyNativeSharpCrossMachineDeterminism.cmake" (
  echo SH44: invalid source root: "%SH44_SOURCE%"
  exit /b 2
)
if not exist "%SH44_SH41%\nsc2-o0-nslink.exe" (
  echo SH44: SH41 products are missing: "%SH44_SH41%"
  exit /b 2
)
if not exist "%SH44_PREFIX%\bin\nslink.exe" (
  echo SH44: NSLink is missing: "%SH44_PREFIX%\bin\nslink.exe"
  exit /b 2
)
if not exist "%SH44_LLVM%\bin\lld-link.exe" (
  echo SH44: LLD is missing: "%SH44_LLVM%\bin\lld-link.exe"
  exit /b 2
)

if not "%SH44_PEER%"=="" (
  if not exist "%SH44_PEER%" (echo SH44: peer attestation is missing: "%SH44_PEER%"& exit /b 2)
  goto :verify_peer
)

:collect
cmake.exe ^
  "-DPYTHON_EXECUTABLE=%SH44_PYTHON%" ^
  "-DSOURCE_ROOT=%SH44_SOURCE%" ^
  "-DOUTPUT_DIR=%SH44_OUTPUT%" ^
  "-DSH41_OUTPUT_DIR=%SH44_SH41%" ^
  "-DLINKER_EXECUTABLE=%SH44_LLVM%\bin\lld-link.exe" ^
  "-DNSLINK_EXECUTABLE=%SH44_PREFIX%\bin\nslink.exe" ^
  "-DRUNTIME_LIBRARY=%SH44_RUNTIME%" ^
  -P "%SH44_SOURCE%\cmake\VerifyNativeSharpCrossMachineDeterminism.cmake"
if errorlevel 1 exit /b %errorlevel%
goto :success

:verify_peer
cmake.exe ^
  "-DPYTHON_EXECUTABLE=%SH44_PYTHON%" ^
  "-DSOURCE_ROOT=%SH44_SOURCE%" ^
  "-DOUTPUT_DIR=%SH44_OUTPUT%" ^
  "-DSH41_OUTPUT_DIR=%SH44_SH41%" ^
  "-DLINKER_EXECUTABLE=%SH44_LLVM%\bin\lld-link.exe" ^
  "-DNSLINK_EXECUTABLE=%SH44_PREFIX%\bin\nslink.exe" ^
  "-DRUNTIME_LIBRARY=%SH44_RUNTIME%" ^
  "-DPEER_ATTESTATION=%SH44_PEER%" ^
  "-DREQUIRE_PEER=ON" ^
  -P "%SH44_SOURCE%\cmake\VerifyNativeSharpCrossMachineDeterminism.cmake"
if errorlevel 1 exit /b %errorlevel%

:success
echo.
echo SH44 report:      %SH44_OUTPUT%\sh44-report.json
echo SH44 attestation: %SH44_OUTPUT%\machine-attestation.json
if "%SH44_PEER%"=="" echo Copy machine-attestation.json to the other machine; it is not a two-machine closure yet.
exit /b 0

:usage
echo Usage:
echo   run_sh44_machine.cmd SOURCE_ROOT SH41_OUTPUT OUTPUT_DIR INSTALL_ROOT LLVM_ROOT [PEER_ATTESTATION]
echo.
echo Example for the development machine:
echo   run_sh44_machine.cmd A:\nsc-devel A:\nsc-devel\selfhost\out\tests-lld\selfhost-sh41 A:\nsc-devel\selfhost\out\sh44-machine A:\native-sharp C:\clang-llvm
echo.
echo The optional sixth argument closes SH44 only when it is a matching attestation from a different machine.
exit /b 2
