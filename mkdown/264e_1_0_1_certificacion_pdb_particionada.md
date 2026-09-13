# Corrección 264E — Certificación PDB particionada / Partitioned PDB certification

## Origen / Origin

El baseline 1/16 reveló primero un error real de ownership de `DISubprogram`, corregido en 264A.
Al repetir únicamente los fixtures afectados con el compilador limpio de 264D, LLVM ya aceptó todas
las unidades, pero `VerifyNativeSharpDebugArtifacts.cmake` siguió rechazando el producto. La causa
era una suposición histórica del fixture: consultaba `llvm-pdbutil` con `--modi=0` y esperaba que el
módulo coordinador contuviera también las funciones ordinarias del usuario.

La generación particionada deliberadamente deja en el módulo cero los globales, cctors, entradas de
destrucción y otros coordinadores; cada función ordinaria conserva sus registros CodeView en la
unidad que posee su definición. El PDB era válido. La certificación estaba observando sólo una parte.

## Implementación / Implementation

The CodeView/PDB verifier now inspects every module in the linked PDB for procedure, type, variable,
scope and source-line evidence. Removing the module-zero filter does not relax any semantic check:
all existing required records remain mandatory, and line-tables-only still rejects `S_LOCAL` and
`S_DEFRANGE` records.

The fixture also accepts explicit optimization-profile and reproducibility-profile inputs. Its
baseline default remains `O0;O2` with O2 reproducibility, while a focused repair run can select O0
only. This keeps the normal certification contract intact and lets compiler development obey the
current O0-only iteration policy.

## Archivos / Files

- `cmake/VerifyNativeSharpDebugArtifacts.cmake`: removes the obsolete `--modi=0` restriction from
  symbol and line inspection, accepts focused profile selection, and reports the profiles actually
  verified.
- `docs/1.0.1/debug/NativeSharp_Debug_Artifact_Contract_1_0_1.md`: makes whole-PDB inspection
  normative for partitioned products.
- `docs/1.0.1/compiler/NativeSharp_Compiler_1_0_1_conformance.map`: traces NSC-DEBUG-008 to both
  partitioned PDB corpora and their verification scripts.
- `docs/1.0.1/CHANGELOG.md`: records the corrected certification behavior.

## Decisiones de diseño / Design decisions

1. No se vuelve monolítica la compilación para satisfacer al fixture.
2. No se copian registros de funciones a módulo cero; eso fabricaría ownership duplicado.
3. No se eliminan checks de variables, rangos, tipos, líneas, SEH, thunks ni checksums.
4. El selector O0 es sólo una entrada del fixture. La ejecución normal conserva O0 y O2.
5. El cambio no modifica el ABI, el lowering ni el formato PDB producido.

## Evidencia dirigida / Focused evidence

- CodeView/PDB completo en O0 con `lld-link`: aprobado.
- CodeView/PDB completo en O0 con `nslink`: aprobado.
- Corpus de debug extremo productor/consumidor en O0 con `lld-link`: aprobado.
- Corpus de debug extremo productor/consumidor en O0 con `nslink`: aprobado.
- No se ejecutó un segundo baseline para la tanda 1.

## Commit de referencia / Reference commit

**Título / Title**

`Certifica PDB particionados completos / Verify complete partitioned PDBs`

**Descripción / Description**

`Inspect symbols and source lines across every PDB module instead of assuming the coordinator owns
ordinary functions, retain all full-debug and line-only assertions, and allow focused O0 evidence
without changing the baseline's default O0/O2 coverage.`
