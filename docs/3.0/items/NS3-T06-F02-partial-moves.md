# NS3-T06-F02 — Partial moves and drop flags

## Resultado

NativeSharp 3.0 mantiene estado `moved`/`maybe-moved` por ruta de field, permite usar hermanos,
rehabilita una ruta al reasignarla y genera drop glue recursivo con guardas únicamente cuando son
necesarias. Un destructor de struct edición 3 ya es ejecutable para tipos move-only y restringe el
move parcial del valor completo que observa.

## Cambios

- Parser: conserva destructores en structs move-only de edición 3; mantiene el rechazo histórico
  de edición 2 y rechaza `copy struct`/`union` con destructor.
- Member model: consulta transitiva única para destructor personalizado y necesidad de value-drop
  a través de fields, tuples y fixed arrays.
- Method bodies: rutas canónicas de locals/parámetros, joins de CFG, back-edges, double move,
  lectura del ancestro, hermanos vivos y rehabilitación por asignación.
- HIR/MIR: regiones `value-local-drop` y `value-parameter-drop`, transferencia del guard al mover
  el valor completo y cleanup compartido con salidas estructuradas/excepciones.
- LLVM: drop recursivo, fields en reversa, destructor propio, null-store de owners y flags `i1`
  sólo para valores con destructor personalizado.
- Particionado LLVM: cada unidad declara los destructores implícitamente invocados por su drop
  glue, incluidos los anidados, sin depender de que exista una llamada fuente explícita.

## Archivos nuevos

- `tests/unit/NativeSharp3T06PartialMoveTests.cpp`: fixture dirigida O0/NSLink con move parcial,
  field hermano, reinicialización, double move, joins, loops, destructor, parámetros, early return,
  unwind y conteo exacto de drops.
- `docs/3.0/language/partial-moves-drop-flags.md`: contrato normativo bilingüe.
- Este archivo: evidencia y mapa del commit.

## Archivos modificados

- `include/nativesharp/Frontend/{ProjectMemberModel.h,ProjectMethodBodyModel.h}`.
- `src/Parser/Parser.cpp`.
- `src/Frontend/{ProjectMemberModel.cpp,ProjectMethodBodyModel.cpp,ProjectOopHirModel.cpp}`.
- `src/LLVM/{ProjectOopCodePartition.cpp,ProjectOopLlvmLowerer.cpp}`.
- `CMakeLists.txt` y las expectativas dirigidas de inicializadores, records y ownership que
  conservan compatibilidad textual con edición 2.

## Mejora observable

Extraer un recurso de un agregado ya no obliga a invalidar artificialmente todos sus fields ni
provoca double drop. El costo runtime se limita a las guardas necesarias; layout y ABI Win64 no
cambian. Los errores distinguen estado movido, estado posible tras join, back-edge y destructor que
requiere el valor completo.

## Validación dirigida

- `nsc.ns3.t06.partial-moves`: 7/7 checks, O0/NSLink, incluida ejecución real de cleanup por scope,
  value parameter, early return y excepción nativa.
- Matriz de regresión directamente afectada: 7/7 tests (`T01 generalized initializers`, `T02 native
  records`, F01, F02, Stage 40 struct ABI, Stage 50.11 ownership y C6 struct members), verde.
- Particionado backend: `nsc.namespace-function-llvm` y `nsc.project-oop-lowering`, 2/2 verdes;
  el segundo enlaza las unidades producidas con NSLink explícito desde A:.
- Construcción incremental de `nsc` y targets dirigidos: correcta con runtime host estático.
- Baseline: no ejecutado por prohibición explícita del proyecto.
