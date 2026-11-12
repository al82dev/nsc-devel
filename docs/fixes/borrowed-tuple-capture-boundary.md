# Captura de tuplas con vistas

Una lambda que captura una tupla con `SharedView` puede sobrevivir al dueño de
la vista. La reproducción histórica aceptó capturas locales por copia y por
referencia para `Rc` y `Arc`; solo se analizaron, sin ejecutar los accesos
inválidos (`A:/nsc-fixes-evidencia/view-tuple-receiver-capture-before.log`).
La conformidad de directorio y herramientas de ese log histórico no está
acreditada; la comparación posterior se identifica por separado abajo.

El cambio candidato extiende las dos guardas existentes de capturas de vista,
para locales y parámetros, con `tupleTypeKeyContainsView`. La declaración y
definición de esa consulta ya existen en la base `db1fea1046528812642685d66a7b984b58df33fb`.
El candidato conserva el rechazo preventivo de capturas y no demuestra el
seguimiento de vistas dentro de otros contenedores.

`tests/integration/borrowed_tuple_capture_boundary.py` conserva las dos
reproducciones locales para cada familia, comprueba una captura de parámetro y
analiza un control válido de tupla ordinaria. Los cinco casos de rechazo exigen
el diagnóstico específico; ninguno de los seis programas se ejecuta. Desde el
repositorio aislado, con `TEMP` y `TMP` en A, se ejecutó:

```
python tests\integration\borrowed_tuple_capture_boundary.py A:\nsc-fixes-evidencia\build\bin\nsc.exe A:\nsc-fixes-evidencia\pointers-readonly-sdk\NativeSharpCore.lib
```

La salida `A:/nsc-fixes-agent1-temp/borrowed-tuple-capture-current.log` registra
los seis resultados esperados. El compilador usado es el build incremental WIP
SHA-256 `15b3e955f7eab1b471bdd2ecdfc82a35b3b1bd65239ffbd5ddd4d91f2cc36c26`;
la biblioteca Core preexistente tiene SHA-256
`565bf2177c85f88d94057ebe8616f2caeb71e01b7e39610bf815c15b685221c6`.
Se configuraron el runtime y el directorio de importación aislados en A y
`NSC_LINKER=nslink.exe`; la prueba solo realiza comprobaciones semánticas.

La evidencia procede del compilador con otros cambios no registrados y de un
SDK binario anterior. No demuestra que esas dos líneas basten sobre HEAD
limpio, ni que la creación y procedencia de la vista sean independientes de
otros cambios WIP. Tampoco acredita alias anidados, otros contenedores, ni
ejecución O0/O2 del control válido.

El 12 de noviembre se retiraron temporalmente solo las dos condiciones de
tupla, tras guardar una copia y su hash, y se compiló incrementalmente el
compilador. Las cinco capturas se aceptaron semánticamente y el control de
tupla ordinaria también. Se restauraron los bytes exactos del archivo, se
reconstruyó incrementalmente y la regresión rechazó las cinco capturas con su
diagnóstico, manteniendo aceptada la tupla ordinaria. No se ejecutaron los
programas potencialmente inseguros. Comandos, hashes y resultados:
`A:/nsc-fixes-evidencia/capture-guard-comparison-20261112/results.json`,
`without-guards-observe.log` y `restored-regression.log` en la misma carpeta.
Esta comparación atribuye el efecto observado a las dos condiciones dentro del
árbol WIP y con ese Core preexistente; no demuestra que toda la rama limpia o
las demás formas de escape estén cubiertas.

El registro opcional de la regresión usa `python` por nombre, directorio del
repositorio y omisión explícita cuando faltan Core o temporales en A. La primera
configuración de CMake tras agregarlo se inició sin fijar explícitamente los
temporales en A, por lo que no se considera conforme; no se verificó si produjo
archivos fuera de A. Se repitió con el entorno controlado y salió 0:
`A:/nsc-fixes-evidencia/capture-configure-controlled-20261112.cmd` y `.log`.
La ejecución directa de la regresión fue la documentada en la comparación;
no se ejecutó CTest para este registro.
