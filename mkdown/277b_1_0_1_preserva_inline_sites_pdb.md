# 277b — Preservación de inline sites en PDB / Preserve PDB inline sites

## Problema / Problem

La partición paralela introducida para acelerar la generación separaba callers y callees antes de
ejecutar el optimizador. En un producto `-g -O2`, LLVM ya no podía realizar esos inlines ni producir
los registros CodeView `S_INLINESITE` que describen el frame lógico. El binario era válido, pero el
PDB perdía frames de `MaybeThrow` y de la lambda de `Main`.

The parallel partition pipeline separated callers and callees before optimization. In a `-g -O2`
product LLVM could no longer perform those inlines or produce the CodeView `S_INLINESITE` records
that describe their logical frames. The binary remained valid, but the PDB lost the `MaybeThrow`
and `Main` lambda frames.

## Implementación / Implementation

- `src/LLVM/ProjectOopObjectGenerator.cpp`
  - `ProjectOopObjectGenerator::generate()` detects full-debug optimized profiles.
  - `-g` combined with O1, O2 or O3 uses the existing whole-module path.
  - O0 full debug remains partitioned; optimized `-g0` and `-gline-tables-only` also remain
    partitioned and parallel.
- `tests/unit/ProjectOopLoweringTests.cpp`
  - Adds a four-worker O2/full-debug assertion proving that the result contains one code unit, one
    object and no false claim of parallel optimization.
- `docs/1.0.1/debug/NativeSharp_Debug_Artifact_Contract_1_0_1.md`
  - Makes inline fidelity normative and documents the temporary whole-module boundary.
- `docs/1.0.1/compiler/NativeSharp_Compiler_Contract_1_0_1.md`
  - Records the exception in the code-generation and parallel-optimization contracts.
- `docs/1.0.1/CHANGELOG.md`
  - Publishes the effective behavior for 1.0.1-dev users.

## Decisión de diseño / Design decision

No se relajó el gate PDB ni se inventaron inline sites. Un registro inline sólo es honesto cuando
el backend realmente integró el callee en el caller y conserva su ubicación anidada. Mantener el
módulo completo para `-g` optimizado sacrifica parte del paralelismo de esa configuración concreta,
pero conserva la semántica de depuración extrema. La partición se podrá reactivar cuando exista
importación de summaries de inlining entre unidades.

The PDB gate was not weakened and no inline sites were fabricated. An inline record is truthful
only when the backend actually integrated the callee into the caller and retained its nested
location. Keeping a whole module for optimized `-g` sacrifices some throughput for that specific
configuration while preserving extreme-debug semantics. Partitioning can be restored after
cross-unit inline-summary import exists.

## Evidencia dirigida / Focused evidence

- `nsc_project_oop_lowering_tests` verifies the generation policy directly.
- `nsc.debug.codeview-pdb` verifies O0/O2 COFF, PDB, source identity, values, ranges, fragments,
  gaps and the two real inline frames with both supported linker configurations.

## Commit de referencia / Reference commit

**Título / Title:** `Preserva inline sites en PDB optimizados / Preserve inline sites in optimized PDBs`

**Descripción / Description:**

`Mantiene monolíticos los productos optimizados con debug completo hasta disponer de summaries entre unidades, preservando frames inline CodeView sin desactivar la partición O0 o de perfiles ligeros. / Keep optimized full-debug products monolithic until cross-unit summaries exist, preserving CodeView inline frames without disabling O0 or lightweight-profile partitioning.`
