# Entorno de la prueba dirigida del driver ASan

La prueba `tests/integration/address_sanitizer_driver.py` conserva sus casos y
aserciones: programa válido e inválido en O0/O2, reconstrucción, publicación de
la DLL, control sin instrumentación y rechazo de opciones y dependencias
incompatibles. No modifica el ejemplo original del reporte.

Antes filtraba el directorio del runtime del `PATH` de sus procesos hijos y
ejecutaba esos procesos desde carpetas temporales. Ahora hereda el `PATH` sin
alterarlo y ejecuta los procesos desde la raíz de esta copia; los proyectos y
salidas siguen en un directorio temporal. El llamador debe dirigir los
temporales a A y proporcionar el compilador propio, Clang y el runtime
autorizados para esta configuración.

Cada invocación registra comando, salida y código de terminación; mientras
espera emite progreso cada 25 segundos y conserva el límite de 120 segundos.
Ambas pruebas registran `ASAN_OPTIONS`, el estado de la ejecución y que sus
fixtures no declaran ni verifican un Core instrumentado. Las ejecuciones
preparadas fijan `detect_leaks=0`, por lo que no prueban ausencia de fugas.

La prueba de publicación de la DLL solo tiene autoridad si el directorio del
runtime no está en el `PATH` heredado. Si está presente, el script informa la
precondición incumplida y devuelve 77 (`SKIP`), no éxito. El mismo estado se
aplica si faltan las variables requeridas o la plataforma no es Windows.

`tests/integration/asan_runtime_path_validation.py` también ejecuta sus
subprocesos desde la raíz de la copia, manteniendo intactas las comprobaciones
de runtimes coincidentes y distintos en O0/O2 y la ejecución del programa.

El análisis sintáctico de ambos scripts mediante `ast.parse` y `git diff
--check` pasaron. El compilador propio `A:/nsc-asan-sdk-evidencia/build/bin/nsc.exe`
tuvo SHA-256 `49b8952fb6e0676b5ab35ca09b19df888e64a332ec3fe88cbcc04d81a868a47d`;
el runtime de Windows producido en el mismo build tuvo SHA-256
`3235a612c2dc62bb05c244885bc135ec76bf847f4f9a700badd9017c1f913a53`.
Se fijaron `NSC_LINKER=nslink.exe`, `NS_LIB_ROOT` y las rutas de runtime e
importaciones aisladas, con temporales en `A:/nsc-asan-driver-evidencia/temp`.

Los comandos `A:/nsc-asan-driver-evidencia/run-driver.cmd` y
`A:/nsc-asan-driver-evidencia/run-runtime-path.cmd` terminaron con código 0 y
sin omisiones. La primera prueba detectó `dynamic-stack-buffer-overflow` en
O0/O2, ejecutó los controles válidos instrumentados y sin instrumentar con
código 42, comprobó la DLL publicada y rechazó las opciones incompatibles y
dependencias incorrectas. La segunda aceptó una copia idéntica de la DLL y
rechazó una copia alterada en O0/O2; los programas válidos devolvieron 42.
Los comandos y salidas íntegros están en `driver.log` y `runtime-path.log` en
esa carpeta. Los manifiestos `hash-before.txt` y `hash-after.txt` incluyen
compilador, runtime, importaciones, Clang y DLL; `fc /b` devolvió 0 sin
diferencias. Los archivos temporales de los fixtures se eliminaron al terminar.

Para contrastar la observación original de ayuda del caso 97, el comando
`A:/nsc-asan-driver-evidencia/run-help.cmd` invocó el mismo `nsc.exe` con
`--help` y devolvió 0; `help.log`, línea 195, muestra `--sanitize-address` y
sus argumentos. `hash-after-help.txt` conserva los mismos hashes. Esto demuestra
que la ausencia histórica en la ayuda ya no se reproduce en este ejecutable;
el ejemplo original con acceso inválido se evalúa en la prueba de heap separada.

Estas pruebas no ejecutaron el ejemplo original de heap del reporte ni prueban
detección dentro de bibliotecas precompiladas o ausencia de fugas. Tampoco
verifican que `nslink.exe` resuelto por PATH pertenezca a este build; se utilizó
como herramienta externa. La cobertura y procedencia de Core se investigan
separadamente.

Observación para el integrador: el banner actual imprime una versión histórica,
aunque el usuario indicó que el producto no debe presentarse mediante versiones
ficticias. Esto se observó en `runtime-path.log`; no se modificó el banner en
este frente de ASan.

El alcance y las restricciones completas constan en
`A:/nsc-asan-evidencia/SCOPE-LOCK.md` y `TAREAS-CHAT-ASAN.md`.
