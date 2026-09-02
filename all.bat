@echo off
setlocal EnableExtensions

rem =====================================================================================
rem NativeSharp -- la corrida completa: checklist de Windows, triage de bugs y manifiesto.
rem
rem DOS CORRECCIONES respecto a la version anterior, y las dos son de comportamiento:
rem
rem   1. `call`. Un .bat que invoca otro .bat SIN `call` transfiere el control y NO VUELVE.
rem      Tal como estaba, este archivo corria run_windows_checklist.bat y terminaba ahi:
rem      run_bug_triage.bat y verify_manifest.py no se ejecutaban nunca.
rem
rem   2. `pushd "%~dp0"`. Los tres pasos se invocaban por nombre suelto, asi que dependian
rem      de que el directorio actual fuera el del repo. Ahora el script se puede llamar
rem      desde cualquier sitio y desde cualquier disco.
rem
rem CRLF OBLIGATORIO en este archivo.
rem =====================================================================================

pushd "%~dp0"

call run_windows_checklist.bat
call run_bug_triage.bat
python verify_manifest.py --clean --write

popd
endlocal
