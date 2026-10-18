# NSC 3.0.5 — M02T: dumper MIR iterativo

## SCOPE LOCK

| Dimensión | Alcance |
|---|---|
| Objetivo | Eliminar la recursión de proceso al serializar árboles `ProjectOopMirNode`. |
| Algoritmo | DFS preorden con pila explícita y alta inversa de hijos. |
| Símbolos | `ProjectOopMirModelDumper::dumpNode`, emisor de línea y recorrido focal compartido. |
| Incluido | Texto, indentación, orden de campos, orden de hijos, slots nulos y saltos de línea. |
| Excluido | Modelo MIR, lowering, caché, cost explorer, destrucción general, API pública, ABI y NSMETA. |

## Contrato preservado

El recorrido histórico emitía primero el nodo actual y después llamaba recursivamente a cada hijo
de izquierda a derecha. Un hijo nulo producía su propia línea indentada `<none>` y no tenía
descendencia. La versión iterativa conserva ese preorden insertando los hijos en la pila de derecha
a izquierda.

El emisor de una línea mantiene, sin reordenarlos, estos grupos contractuales:

1. clase de nodo, modo de coma flotante, tipo, identidades y ordinales;
2. catch, cleanup, borrow, parámetros y construcción;
3. RTTI, campos, bitfields, aggregates, colecciones, arrays y storage;
4. assembly integrado, referencias y control estructurado;
5. ownership, destrucción, callees, propagación de `Result`, operación y conversión.

La extracción en helpers locales evita sustituir la recursión por otro monolito: cada helper nuevo
queda por debajo de 60 líneas ejecutables. No se declara un tipo adicional; el worklist usa pares
`(node, depth)`.

## Profundidad y tamaño de salida

La indentación histórica son dos espacios por nivel. Por ello una cadena de profundidad `H` genera
`O(H²)` bytes aunque el recorrido sea lineal en nodos. Un dump materializado de 32,768 niveles
contendría aproximadamente 1 GiB sólo de espacios y `ostringstream::str()` podría requerir otra
copia de tamaño comparable.

El contrafactual separa dos responsabilidades sin debilitar el contrato:

- el mismo walker usado por producción recorre 32,768 aristas, cuenta todos los nodos y el slot
  nulo y demuestra que la profundidad ya no consume la pila del proceso;
- el dump real materializa 8,192 aristas, profundidad que ya reproduce el riesgo de stack overflow
  y mantiene el costo de evidencia dentro de una escala proporcional;
- un árbol pequeño ramificado compara el texto completo byte por byte, incluidos campos,
  indentación, preorden y dos slots nulos en niveles distintos.

La destrucción de los árboles de prueba se hace con la utilidad iterativa ya existente para que
M02T no reclame ni adelante M02V.

## Infraestructura y gates

No se agrega una fuente ni un ejecutable de prueba. La cobertura vive en
`tests/unit/ProjectOopLoweringTests.cpp`, ya compilado por `nsc_project_oop_lowering_tests` y
registrado como `nsc.project-oop-lowering`.

- Foco nuevo: `NSC_PROJECT_OOP_LOWERING_FOCUS=mir-dump-depth`.
- Ruta default: ejecuta únicamente la comparación byte-exacta pequeña; el caso de 8,192 niveles
  permanece focal para no añadir decenas de MiB a cada corrida ordinaria.
- Regresiones aplicables: contrato `CF-FRONT-063` (`TF-FRONT-497`…`TF-FRONT-504`) y los CTest
  `nsc.stage35-recovery.mir-dump`, `nsc.stage36-recovery.mir-dump` y
  `nsc.stage37/38/39/40/42/43/44.mir-dump`.
- `validate_source_registration.py` debe seguir pasando; no aplican catálogo, manifiesto ni Builder
  nuevos porque no existe un nuevo archivo de prueba.

## Estado de validación

La implementación, la auditoría estática y los gates focales quedaron cerrados:

- `nsc` construido en `build-clang`; contrato estático host/runtime PASS
  (`A:\temp\m02t-build-nsc.log`).
- `nsc_project_oop_lowering_tests` construido; foco `mir-dump-depth` PASS 3/3
  (`A:\temp\m02t-build-lowering.log`, `A:\temp\m02t-focus-mir-dump-depth.log`).
- ruta predeterminada `nsc.project-oop-lowering` PASS 1/1 en 5.72 s
  (`A:\temp\m02t-ctest-lowering.log`).
- los nueve CTest `*.mir-dump` de Stage 35, 36, 37, 38, 39, 40, 42, 43 y 44 pasaron 9/9
  (`A:\temp\m02t-ctest-mir-dumps.log`).
- `nsc_component_tests` construido y `TF-FRONT-497`…`TF-FRONT-504` pasaron 8/8 de forma
  individual (`A:\temp\m02t-build-component-tests.log`,
  `A:\temp\m02t-tf-front-497-504.log`).
- `validate_source_registration.py` PASS
  (`A:\temp\m02t-source-registration.log`).

La auditoría old-vs-new confirmó 166/166 literales y 219/219 accesos de campos MIR en el mismo
orden. `dumpNode` no conserva self-call y todos los helpers nuevos quedan debajo de 60 líneas
ejecutables. No se ejecutó baseline, deploy, instalación ni batería completa.
