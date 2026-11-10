# Dependencias de layout en campos de tupla

Una estructura `AOuter` con un campo `(ZInner, int)` fallaba al calcular su
layout si `AOuter` se procesaba antes que `ZInner`. La misma estructura con un
campo directo `ZInner` funcionaba, al igual que una tupla cuyo elemento ya se
había procesado. El fallo impedía compilar desde fuentes una estructura que
contenía una tupla con `SharedView`; el control con Core binario sí ejecutaba.
Reproducción y controles: `A:/nsc-fixes-evidencia/tuple-field-layout-test-before.log`
y `view-tuple-storage-directed-proto7.log`.

`ProjectObjectLayoutModel::layoutBaseType` prepara ahora los layouts por valor
de los elementos del campo de tupla antes de consultar su layout agregado. La
exploración de tuplas anidadas usa una pila y reutiliza la ruta de layout de
campos y su detección de ciclos; no recalcula por separado tamaños ni offsets.
La prueba `tests/integration/tuple_field_layout_dependencies.py` ejecutó 42 en
O0/O2 para el orden problemático, el inverso, una tupla anidada, el campo directo
y un ciclo mediante puntero. Los ciclos directos e indirectos por valor se
rechazaron (`tuple-field-layout-test-restored.log`). Las pruebas dirigidas
`nsc.stage29.project-layouts` y `nsc.stage40.layouts` pasaron 1/1 cada una; la
selección de vistas con Core desde fuentes también pasó, incluido el control
que ejecuta 42 (`view-tuple-storage-directed-restored.log`).

La prueba unitaria `nsc.project-object-layout-model` falló 3 de 47 comprobaciones
de diagnósticos para estructuras recursivas y arreglos inválidos. Se retiró
temporalmente solo este cambio, se reconstruyó esa prueba y se repitió: los
mismos tres fallos persistieron (`tuple-field-layout-unit.log` y
`tuple-field-layout-unit-without.log`). Después se restauró el cambio, se
reconstruyó incrementalmente y se repitieron los controles citados. En ese
momento los fallos unitarios permanecían abiertos. El build
incluye otros cambios locales sin registrar y no certifica un árbol limpio.

La investigación posterior identificó la causa: cada fixture devolvía un
objeto recién creado mediante un retorno sin `owned`, por lo que el análisis
de propiedad detectaba la fuga antes del cálculo de layout. La comparación con
retorno `owned` o sin el método está en
`A:/nsc-fixes-evidencia/layout-unit-diagnostic-probe.log`: las variantes válidas
llegan al diagnóstico de recursión o conteo de arreglo esperado. Se añadió
`owned` a las tres cadenas de fixture sin cambiar sus aserciones ni los ejemplos
del reporte. La prueba unitaria pasó sus 47 comprobaciones
(`layout-unit-fixture-check.log`); también pasaron la regresión de tuplas y los
controles dirigidos de layout (`tuple-field-layout-test-fixture.log`,
`layout-unit-fixture-stage29.log` y `layout-unit-fixture-stage40.log`).

El archivo de layout es compartido por HIR, MIR y metadata y puede coincidir
con cambios de la rama de interoperabilidad. Esta tanda no modifica código ni
builds de interoperabilidad C/C++ y no prueba su ABI.
