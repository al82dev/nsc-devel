# 298 — Representación de asm en MIR / Assembly MIR representation

## Resultado / Result

El pipeline de proyecto ya no rechaza ni borra assembly validado. Cada sentencia llega a HIR y
MIR como una operación propia que conserva el contrato completo necesario para emitirla después. /
The project pipeline no longer rejects or erases validated assembly. Every statement reaches HIR
and MIR as a first-class operation preserving the complete contract required by later emission.

## Archivos y funciones / Files and functions

- `include/nativesharp/Frontend/ProjectOopHirModel.h`
  - Agrega nodos para bloque, línea, input, output y clobber de assembly.
- `src/Frontend/ProjectOopHirModel.cpp`
  - `lowerStatement()` convierte el AST validado sin reparsing.
  - `projectOopHirNodeKindName()` publica identidades legibles y estables.
- `include/nativesharp/Frontend/ProjectOopMirModel.h`
  - Agrega los cinco nodos MIR y eleva el schema interno de 1.0 a 1.1.
- `src/Frontend/ProjectOopMirModel.cpp`
  - Transporta cada forma HIR a su equivalente MIR sin perder datos.
- `src/Frontend/ProjectMethodBodyModel.cpp`
  - Abre la frontera semántica del pipeline de proyecto después de validar operands.
- `src/Frontend/ProjectOopHirModelDumper.cpp` y `ProjectOopMirModelDumper.cpp`
  - Muestran constraints, ordinales, categorías, preservación y tamaño del source.
- `tests/unit/IntegratedAssemblyMirTests.cpp`
  - Certifica el contrato estructurado completo y sus dumps.
- `CMakeLists.txt` y `docs/1.0.1`
  - Registran la prueba y actualizan especificación, contrato, trazabilidad, estado y roadmap.

## Implementación y decisiones / Implementation and design decisions

Los hijos del bloque conservan el orden fuente original. Una entrada `r`/`i` contiene un valor;
una entrada `m` y cualquier output contienen una dirección de almacenamiento. Así LLVM no tendrá
que reconstruir lvalues. Constraints normalizados, explicitness, categorías de clobber y marcas de
preservación viajan como datos, no como inferencias desde el texto. / Block children preserve
original source order. An `r`/`i` input owns a value; an `m` input and every output own a storage
address. LLVM therefore will not need to reconstruct lvalues. Normalized constraints, explicitness,
clobber categories, and preservation markers travel as data rather than text-derived guesses.

El frontend aislado conserva temporalmente su diagnóstico porque no participa en el backend OOP;
el pipeline de proyecto, que sí produce MIR y binarios, ya acepta la operación. / The standalone
frontend temporarily retains its diagnostic because it does not participate in the OOP backend;
the project pipeline, which produces MIR and binaries, now accepts the operation.

## Evidencia enfocada / Focused evidence

```text
nsc.integrated-assembly.grammar: passed
nsc.integrated-assembly.mir: PASS: 7 checks
```

No se ejecutó baseline. / No baseline was run.

## Mensaje de commit / Commit message

**Título / Title**

`Representa asm como contrato estructurado en HIR y MIR / Represent asm as a structured HIR and MIR contract`

**Descripción / Description**

`Preserva texto, ubicaciones, operands, constraints y clobbers en MIR 1.1 sin volver a interpretar el source. / Preserve text, locations, operands, constraints, and clobbers in MIR 1.1 without reparsing source.`
