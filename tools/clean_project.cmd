@echo off
setlocal EnableExtensions
if "%~1"=="" exit /b 2
set "DIR=%~f1"
if not exist "%DIR%" exit /b 0
del /q "%DIR%\*.exe" 2>nul
del /q "%DIR%\*.obj" 2>nul
del /q "%DIR%\*.pdb" 2>nul
del /q "%DIR%\*.ilk" 2>nul
del /q "%DIR%\*.ll" 2>nul
del /q "%DIR%\*.mir" 2>nul
del /q "%DIR%\*.hir" 2>nul
exit /b 0
