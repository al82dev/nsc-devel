@echo off
setlocal EnableExtensions
rem =====================================================================================
rem NativeSharp -- LA CORRIDA. Un comando: limpia, construye, prueba, mide y deja UN archivo.
rem
rem     all.bat            corrida completa
rem     all.bat quick      lo que run_windows_checklist.bat entienda por rapido
rem
rem QUE DEJA, Y ES LO UNICO QUE HAY QUE ABRIR:
rem
rem     _logs\REPORTE.txt
rem
rem Autocontenido a proposito. Si algo fallo, el fragmento del log que lo explica esta DENTRO
rem del reporte, no referenciado desde el. Los logs crudos siguen en _logs\ por si hace falta
rem bajar mas, pero leerlos ya no es el camino normal.
rem
rem -------------------------------------------------------------------------------------
rem POR QUE ESTE ARCHIVO ES ASI, Y NO COMO ESTABA
rem
rem   1. `call`. Un .bat que invoca otro .bat SIN `call` transfiere el control y NO VUELVE.
rem      Sin el, este archivo terminaba en el primer paso.
rem
rem   2. `pushd "%~dp0"`. Los pasos se invocan por nombre suelto, asi que dependian de que el
rem      directorio actual fuera el del repo. Ahora se puede llamar desde cualquier sitio y
rem      desde cualquier disco.
rem
rem   3. EL CHECKLIST SE LLAMA UNA SOLA VEZ. Antes se llamaba dos, y era peor que perder
rem      siete minutos: su PRIMERA accion es `rmdir /s /q _logs` (linea 58 de
rem      run_windows_checklist.bat), asi que la segunda pasada BORRABA los logs de la primera.
rem      Los mtimes de _logs quedaban separados por hora y media dentro de un directorio que
rem      se borra entero en cada arranque -- un estado imposible que costo tiempo de leer.
rem
rem   4. NO SE LLAMA A run_bug_triage.bat POR SEPARADO. El checklist YA lo llama, en la linea
rem      334, y ademas redirige su salida a _logs\05_triage_run.log. Llamarlo aparte lo corria
rem      dos veces y mandaba la segunda salida a la consola, donde se pierde.
rem
rem   5. EL ORDEN IMPORTA. El checklist BORRA _logs al arrancar, asi que todo lo que quiera
rem      dejar rastro tiene que correr DESPUES. De ahi: checklist, luego manifiesto (con su
rem      salida capturada a _logs\06_manifest.log), luego el reporte, que lo lee todo.
rem
rem   6. EL CODIGO DE SALIDA ES EL DE report.py. Devuelve 1 si hay cualquier fallo o si la
rem      corrida murio a la mitad, asi que `all.bat` puede decir la verdad en una linea.
rem
rem CRLF OBLIGATORIO en este archivo -- cmd.exe resuelve los saltos por posicion de byte.
rem =====================================================================================
pushd "%~dp0"

call run_windows_checklist.bat %*

if not exist "_logs" mkdir "_logs"
python verify_manifest.py --clean --write > "_logs\06_manifest.log" 2>&1

python report.py
set "RC=%ERRORLEVEL%"

echo.
echo =====================================================================================
type "_logs\REPORTE.txt"
echo =====================================================================================
echo.
if "%RC%"=="0" (
    echo TODO EN VERDE. El detalle esta en _logs\REPORTE.txt
) else (
    echo HAY FALLOS. Todo lo necesario para leerlos esta en _logs\REPORTE.txt
)

popd
endlocal & exit /b %RC%
