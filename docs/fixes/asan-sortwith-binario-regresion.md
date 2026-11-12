# Regresión reproducible de `SortWith` con SDK binario

`tests/integration/sort_with_binary_original.py` proporciona una prueba dirigida
del ejemplo 100 sin modificar su fuente. Recibe el compilador, el archivo
original, los pares `.lib`/`.nsmeta` de Core y Collections y un directorio
temporal explícitamente; copia la fuente byte por byte, genera un proyecto
consumidor con dos `LibraryReference`, y comprueba compilación y ejecución con
salida cero en O0 y O2. Sus procesos hijos arrancan en la raíz del repositorio.
La opción obligatoria `--mode` selecciona `normal`, `asan` o `both`; los modos
con ASan requieren Clang y su runtime como argumentos, de forma que una falta de
configuración no produzca un éxito aparente.

Cada ejecución imprime comandos, directorio de inicio, salida y código de cada
proceso, y los SHA-256 de las entradas antes y después. Comprueba el hash de la
copia original, desactiva la caché de artefactos y devuelve 1 ante cualquier
fallo. Los temporales se crean bajo `--temp-root` y se eliminan al terminar.
El registro de esta prueba en CMake queda para el integrador: esta rama no
modifica `CMakeLists.txt`.

## Ejecución dirigida sobre el compilador de esta copia

`A:/nsc-asan-evidencia/run-sortwith-binary-baseline.cmd`, iniciado desde
`A:/nsc-limitaciones-asan`, utiliza el compilador propio SHA-256
`49b8952fb6e0676b5ab35ca09b19df888e64a332ec3fe88cbcc04d81a868a47d`, Core y
Collections binarios de `A:/nsc-asan-sdk-evidencia`, el original del reporte y
temporales de A. El comando completo, configuración, hashes y salida están en
`A:/nsc-asan-evidencia/sortwith-binary-baseline.log`.

La prueba devolvió **1**: las cuatro combinaciones normal/ASan por O0/O2
fallaron durante la compilación por la imposibilidad de inferir `SortWith`.
No hubo ejecutables para esas combinaciones. Las entradas mantuvieron sus
hashes, incluido el ejemplo original. Es una regresión detectada, no una
prueba aprobada ni una corrección del compilador.

Para comprobar que el ejecutor puede atravesar compilación y ejecución se
utilizó el control separado que escribe `SortWith<int>`; su fuente no sustituye
el ejemplo original. `A:/nsc-asan-evidencia/run-sortwith-binary-positive-control.cmd`
terminó 0 y las cuatro compilaciones y ejecuciones terminaron 0. El registro es
`A:/nsc-asan-evidencia/sortwith-binary-positive-control.log`. La comprobación
sintáctica del ejecutor mediante `python -B -m ast` terminó 0. El control
`run-sortwith-binary-missing-asan.cmd` solicitó `both` sin Clang ni runtime y
terminó 2 con el diagnóstico de configuración requerido; no inició compilación.
Su salida está en `sortwith-binary-missing-asan.log`.

Estas ejecuciones usaron `ASAN_OPTIONS=detect_leaks=0`, y las bibliotecas Core
y Collections no estaban instrumentadas. El control explícito demuestra el
recorrido concreto del ejecutor, no la corrección de inferencia ni la ausencia
general de fugas. La comprobación de la prueba original permanece pendiente
hasta integrar y validar una corrección del frontend de genéricos; las causas,
controles de identidad de tipos y archivos compartidos figuran en
`asan-sortwith-binario-handoff.md`. El `SCOPE LOCK` de la campaña se conserva
en `A:/nsc-asan-evidencia/SCOPE-LOCK.md`; no se tocó interoperabilidad.
