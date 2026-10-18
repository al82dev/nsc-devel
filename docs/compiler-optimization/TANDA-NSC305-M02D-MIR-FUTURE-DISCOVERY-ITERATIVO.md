# Tanda NSC 3.0.5 M02D — descubrimiento iterativo de layouts future

Fecha: 2026-10-18
Rama: `development`

## SCOPE LOCK

M02D migra exclusivamente el recorrido local `collectFutureNode` de
`ProjectOopMirModel::importLayouts`. La hoja fue descubierta al auditar la recursión MIR residual
después de M02C. No modifica la detección de claves `future:<T>`, el cálculo físico de layouts,
la colección de tuples o iterators, HIR, MIR, sintaxis, ABI, NSMETA, diagnósticos ni LLVM.

`collectIteratorNode` conserva por ahora su implementación histórica y constituye una hoja
posterior separada. M02D depende de M02A/M02B sólo porque comparte la unidad
`MirLayoutImporter.cpp`; no altera sus worklists ni el lowering de nodos cerrado en M02C.

## Conducta histórica preservada

El lambda recursivo omitía un nodo nulo; para cada nodo presente visitaba `typeKey`,
`elementTypeKey` y `argumentTypeKeys` en ese orden, y después descendía por los hijos de izquierda
a derecha. Las claves reconocidas se acumulaban con repeticiones y sólo al terminar se aplicaban
`sort` y `unique`. El cálculo de campos, offsets, tamaños, alineaciones, `pollFn` y error tipado se
ejecutaba después del descubrimiento.

La ruta iterativa se extrajo como el helper privado productivo `collectFutureLayoutKeys` y usa un
worklist LIFO de punteros HIR no propietarios. Inserta los hijos en orden inverso para recuperar al
extraerlos el mismo preorden padre→hijos de izquierda a derecha. Los slots nulos se insertan y se
omiten al extraer, reproduciendo el retorno temprano histórico. No se deduplica durante el recorrido
y no cambia la frontera posterior de ordenamiento.

## Contrafactual profundo

El focus `mir-future-layout-depth` usa el acceso privado de pruebas ya existente y llama directamente
el helper productivo. Construye una cadena de 32,768 nodos HIR con un slot nulo, coloca claves future
duplicadas tanto en la raíz como en la hoja más profunda y comprueba:

- terminación sin recursión de stack;
- descubrimiento de ambas claves profundas;
- preorden exacto y conservación de duplicados antes del ordenamiento.

Una segunda prueba somera llama `importLayouts` y comprueba `sort` y deduplicación deterministas,
cuatro campos físicos por future y los tipos exactos `builtin:int` y `builtin:long` de los campos de
resultado.

El primer diseño del contrafactual hizo profunda la llamada completa a `importLayouts`. La ruta
future iterativa terminó, pero el proceso cayó con `0xC00000FD` al alcanzar después el recorrido
recursivo `collectIteratorNode`, reservado a M02E. Esa ejecución no se presenta como PASS M02D; se
conserva en `A:\temp\m02d-focus-future-layout-depth.log` como evidencia causal de la hoja siguiente.
El fixture se separó para probar profundamente sólo el helper future y mantener somera la cobertura
integrada, sin migrar ni alterar iterator en este commit.

La prueba se registra como selector y llamada default dentro de
`nsc_project_oop_lowering_tests`. No crea fuente, ejecutable, target, caso de catálogo ni entrada de
manifiesto. El target, su fuente y `add_test(NAME nsc.project-oop-lowering ...)` ya están registrados
exactamente una vez. `MirLayoutImporter.cpp` continúa registrado una vez en el CMake raíz y una vez
en la compiler-suite.

## Límites estáticos y evidencia

`collectFutureLayoutKeys` queda sin self-recursion y por debajo de 60 statements ejecutables. La
prueba y los helpers añadidos permanecen también bajo ese límite. La validación se ejecutó con
`TEMP` y `TMP` en `A:\temp`:

- primer build compartido: FAIL por el error R01I aislado
  `AttributeParser.cpp:59 bool state.isAsyncMethod`; log `A:\temp\m02d-build-nsc.log`;
- build incremental después del fix R01I: PASS; log
  `A:\temp\m02d-build-nsc-r01i-fix1.log`;
- primer focus profundo integrado: `0xC00000FD` por `collectIteratorNode`, como se documentó arriba;
- build final `nsc`: PASS y contrato de runtime estático aprobado; log
  `A:\temp\m02d-build-nsc-final.log`;
- build final `nsc_project_oop_lowering_tests`: PASS; log
  `A:\temp\m02d-build-lowering-isolated.log`;
- focus `mir-future-layout-depth`: PASS, 4 checks, exit 0, 290 ms; log
  `A:\temp\m02d-focus-future-layout-depth-isolated.log`;
- CTest `nsc.project-oop-lowering`: PASS, 164 checks, 1/1, 5.98 s; log
  `A:\temp\m02d-ctest-project-oop-lowering.log`;
- guard auxiliar del contrato async `nsc_ns3_t10_async_function_tests`: PASS, 9 checks directo y
  CTest 1/1. Este guard valida la planificación async, no sustituye la prueba productiva de
  `importLayouts`. Logs `A:\temp\m02d-direct-t10-async-functions.log` y
  `A:\temp\m02d-ctest-t10-async-functions.log`.

No se ejecutó baseline ni suite completa. La evidencia de M02D es el helper productivo profundo y
la publicación somera dentro del runner lowering; el guard T10 se conserva sólo como evidencia
auxiliar y no se presenta como consumidor directo del importer MIR.
