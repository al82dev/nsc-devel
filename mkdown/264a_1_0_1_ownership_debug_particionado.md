# Corrección 264A — Ownership de debug particionado / Partitioned debug ownership

## Origen / Origin

El baseline 1/16 (`20260912-222114-1bf38198`) mostró que los módulos LLVM particionados adjuntaban
un `DISubprogram` de definición tanto al dueño de una función como a las declaraciones de los otros
módulos. LLVM rechazaba esas declaraciones con `function declaration may only have a unique !dbg
attachment`, bloqueando CodeView/PDB antes del enlace.

## Implementación / Implementation

`ProjectOopLlvmLowerer` now computes definition ownership before creating debug metadata. Ordinary
MIR functions receive a subprogram only in their assigned code unit. Generated type initializers and
destruction entries receive one only in the coordinator module that defines them. Peer declarations
retain their ABI declaration and attributes but no fabricated definition metadata.

La corrección no desactiva debug ni desparticiona el programa. Cada unidad sigue emitiendo sus
propias funciones con CodeView completo, mientras el coordinador y las unidades pares pueden
referenciarlas de manera normal.

## Archivos / Files

- `src/LLVM/ProjectOopLlvmLowerer.cpp`: gates debug-subprogram creation on physical definition
  ownership for functions, cctors and destruction entries.
- `tests/unit/ProjectOopLoweringTests.cpp`: verifies coordinator and every code unit independently
  under the full debug profile.
- Debug contract and conformance map: normative ownership and traceability.

## Evidencia dirigida / Focused evidence

- OOP lowering: 92 checks pass.
- A partitioned `DebugPdb` OOP object passes LLVM and COFF verification at O0/full.
- The object retains `.debug$S` and `.debug$T` sections.
- El siguiente fallo del enlace queda aislado como identidad duplicada de helpers de closures; no se
  confunde con esta corrección.
- Clean compiler-only rebuild passes; no additional baseline was run.

## Commit de referencia / Reference commit

**Título / Title**

`Corrige ownership de debug en módulos particionados / Fix debug ownership in partitioned modules`

**Descripción / Description**

`Attach definition subprogram metadata only in the LLVM module that owns each physical definition,
leave peer declarations free of fabricated debug definitions, cover generated cctors and destruction
entries, and verify every full-debug partition independently.`
