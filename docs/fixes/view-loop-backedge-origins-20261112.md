# Orígenes de vistas en iteraciones posteriores

En `8e83dc36d`, una vista podía transferirse a un destino exterior mediante
`ref/out` al comienzo de un ciclo y cambiar al final a una vista de un dueño
interior. El análisis de una sola visita aceptaba el programa, aunque otra
iteración podía transferir la segunda vista fuera de su dueño. Se reprodujo
para `while` y `for` con `ref` y `out` sin ejecutar programas negativos:
`A:/nsc-fixes-evidencia/view-backedge-real-20261112.log` y
`A:/nsc-fixes-evidencia/view-for-backedge-before-20261112.log`.

Al completar un `while`, `do-while` o `for` con backedge alcanzable se vuelven
a comprobar las llamadas resueltas que tienen salidas de vista y las
asignaciones de vista con binding local conocido. Se reutilizan los
comprobadores de asignación y salida; las raíces posibles se acumulan hasta que
dejan de cambiar. No se supone que Unknown sea una identidad local. El
análisis usa un recorrido iterativo de la sintaxis y no se ejecuta cuando el
ciclo no contiene llamadas aplicables. Se hace para ambos recorridos de análisis
de ciclos y solo dentro del callable correspondiente.

Una sola pasada resultó insuficiente: una cadena de vistas transportadas por
llamadas fue aceptada antes del punto fijo, y una cadena de asignaciones
directas también se aceptó antes de añadir esas escrituras. Las reproducciones
están en `view-while-chain-before2-20261112.log` y
`view-loop-assignment-chain-before-20261112.log` bajo
`A:/nsc-fixes-evidencia`. La regresión dirigida final contiene esos casos y
60 comprobaciones pasaron (`view-assignment-fixedpoint-final-20261112.log`),
incluidos controles positivos, alias, `do-while`, `for` y un `foreach` sobre
arreglo. Los escapes negativos se analizaron, no se ejecutaron. También pasaron
las pruebas dirigidas de shadowing y tuplas.

La compilación incremental produjo el nsc aislado con SHA-256
`f7bb6e74acd03c1aa849c0f7a1ff02c0c2df34ee410d6146857b8571797fb7ab`.
En O0/O2 ejecutaron los controles positivos de cadenas, `while`, `for` y
`foreach` con el resultado esperado. Una excepción es el control positivo de
`for` con `break` incondicional: su análisis pasa, pero falla la generación
de código para la actualización del bucle en O0/O2. Se reprodujo también sin
vistas (`for-break-binding-before-20261112.log`); queda pendiente de corrección
separada. Resultados completos de ejecución, incluido el fallo, en
`view-loops-positive-runtime-assignment-20261112.log`.
La corrección posterior de esa falla y su validación están documentadas en
`for-unreachable-update-binding-20261112.md`; se conserva aquí el resultado
anterior para no perder la secuencia de evidencia.

La unión histórica es conservadora y puede rechazar programas válidos. Esta
comprobación no acredita todas las llamadas indirectas o de namespace,
proyecciones, escrituras sin binding, expresiones alojadas en sintaxis de tipo
ni procedencia Unknown. Tampoco resuelve el falso rechazo de shadowing. No se
ejecutó batería completa ni interoperabilidad C/C++.
