# NSC 3.0.5 — H02S13I: asignación deconstruida HIR iterativa

## Clasificación

`IMPLEMENTADO Y VALIDADO FOCALMENTE`.

## Alcance cerrado

- Migra únicamente `AssignmentExpression` con token `=` y patrón tuple semánticamente ligado.
- Conserva la ruta histórica para contexto de dirección, binding ausente, `base` implícito y
  estados inválidos que deben mantener sus diagnósticos actuales.
- No cambia parser, AST, Sema, bindings, sintaxis, API, ABI, NSMETA, MIR ni LLVM.
- No invade eventos, setters, asignaciones directas ni las demás hojas H02S13.

## Causa

El walker `visitHirTuplePatternLeaves` ya era iterativo, pero la asignación completa permanecía
fuera de la máquina: `lowerDeconstructionSource` reentraba para bajar la fuente y el callback de
cada hoja reentraba para bajar la dirección destino. Una cadena de asignaciones deconstruidas usada
como fuente consumía un frame nativo por nivel.

## Diseño

La máquina incorpora `DeconstructionAssignment` y conserva una sola continuación heap-backed:

1. baja la fuente antes de observar cualquier destino;
2. para un `Deconstruct` nominal conserva receptor, materialización struct y `memberOrdinal`;
3. crea `TupleDeconstruct` con la fuente como hijo cero;
4. aplana el patrón con el walker existente sin bajar expresiones dentro de sus callbacks;
5. omite `_` y programa una dirección destino por vez en contexto de dirección;
6. al reanudar clasifica la conversión y anexa `TupleDeconstructElement`;
7. publica la raíz sólo después de consumir todas las hojas en orden.

## Invariantes auditadas

- La fuente se evalúa exactamente una vez y antes que todos los destinos.
- Los destinos conservan preorder izquierda a derecha; `_` no produce dirección ni HIR.
- Se preservan tipo de origen, offset absoluto, tipo destino, conversión y una sola dirección hija.
- La fuente nominal conserva `InstanceCall`, tipo tuple, receptor y ordinal seleccionado.
- El primer error detiene la máquina y mantiene el rango de la asignación cuando no existe uno más
  específico.
- La ruta histórica conserva literalmente `tuple deconstruction result is a value, not addressable
  storage` y todos los diagnósticos `T02-F03` de estados inválidos.
- No queda llamada a `lowerDeconstructionSource` ni a `lowerExpression(target, ..., true, ...)` en
  la ruta válida clasificada por el frame.

## Prueba puntual propia

`NSC_PROJECT_OOP_LOWERING_FOCUS=hir-deconstruction-assignment-depth`

El fixture construye 32,768 asignaciones deconstruidas enlazadas por la arista de fuente. El oracle
recorre el HIR iterativamente y exige en cada nivel `TupleDeconstruct`, fuente como hijo cero,
`TupleDeconstructElement` posterior, offset cero, conversión identidad y una sola `LocalAddress`.
El último nodo debe ser el único `LocalValue` de la fuente, lo que demuestra materialización única y
orden fuente-antes-de-destino sin depender de la pila nativa.

No se añade fuente ni target: el archivo ya pertenece a `nsc_project_oop_lowering_tests`; no aplican
CMake, CTest, manifiesto, catálogo ni NSC Builder 3.

## Validación autorizada

Se compilará únicamente `nsc_project_oop_lowering_tests` y se ejecutará únicamente el selector
`hir-deconstruction-assignment-depth`. No se ejecutarán ruta default, CTest, TL, regresiones,
minisuite, suite completa ni baseline.

## Evidencia

- Primer build: detectó únicamente un `const AstNode*` incompatible y dos inicializaciones
  parciales dentro del fixture nuevo; corregidos sin modificar producción adicional,
  `A:\NSC-devel-paralelo\logs\agente-1\h02s13i-build.log`.
- Target `nsc_project_oop_lowering_tests`: PASS, exit 0,
  `A:\NSC-devel-paralelo\logs\agente-1\h02s13i-build-final.log`.
- Selector `hir-deconstruction-assignment-depth`: PASS, 2 checks, exit 0,
  `A:\NSC-devel-paralelo\logs\agente-1\h02s13i-focus.log`.
- No se ejecutaron ruta default, CTest, TL, regresiones, minisuite, suite completa ni baseline.
