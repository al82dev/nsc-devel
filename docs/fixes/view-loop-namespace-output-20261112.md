# Salidas de vista por funciones globales dentro de ciclos

En `52e5aa7ad`, los dos controles de `view-namespace-backedge-20261112.py`
aceptaron sin diagnóstico salidas `ref` y `out` que podían transferir una
vista del dueño interior en una iteración posterior. El control con ambos
dueños en el ámbito de destino fue aceptado. Los negativos se analizaron sin
ejecutarlos; salida anterior en
`A:/nsc-fixes-evidencia/view-namespace-backedge-before-20261112.log`.

El control de backedge consultaba `calls_`, pero las funciones globales se
registran en `namespaceFunctionCalls_`, aunque ambas rutas usan
`recordViewOutputOrigins`. La revisión incluye ahora las llamadas globales
resueltas del callable y su adaptador de parámetros en el mismo punto fijo de
orígenes utilizado para métodos y asignaciones. Se preservan los límites
conservadores y se omite la repetición si no hay backedge alcanzable.

Tras compilar incrementalmente el target `nsc`, el compilador aislado tuvo
SHA-256 `0920bf761f80caf940ad0ae4fa38ba34f5c2adb761f5256d5a23752ed768624c`.
La prueba dirigida `view_namespace_loop_outputs.py` pasó seis casos: rechazos
ref/out, `for` y cadena entre función global y método, más dos controles
positivos. Los negativos conservaron el diagnóstico `NS3020` del dueño
interior; no se ejecutaron. Resultados en
`A:/nsc-fixes-evidencia/view-namespace-backedge-after-20261112.log`.
Los dos controles positivos se compilaron y ejecutaron en O0/O2 con resultados
7 y 42 (`view-namespace-positive-runtime-20261112.log`). Las regresiones
dirigidas de métodos y shadowing también pasaron.

Esta corrección no cubre por sí sola llamadas indirectas, destinos con alias
desconocido, todos los productores o sintaxis oculta en tipos. No se ejecutó
batería completa ni se modificó interoperabilidad C/C++.
