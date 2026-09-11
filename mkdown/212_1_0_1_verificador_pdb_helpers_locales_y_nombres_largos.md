# Verificador PDB para helpers locales y nombres largos / PDB verifier for local helpers and long names

## Resumen / Summary

El artefacto PDB O0 contenía la superficie de depuración requerida, pero el script de certificación lo rechazaba por dos supuestos de presentación que ya no representan el contrato semántico:

- Un filtro de excepción generado internamente aparecía como `S_LPROC32`, no como procedimiento global.
- `llvm-pdbutil` partía el nombre físico largo de un thunk C en la línea siguiente.

The O0 PDB contained the required debug surface, but the certification script rejected it because of two presentation assumptions that no longer represented the semantic contract: an internal exception filter was emitted as `S_LPROC32`, and `llvm-pdbutil` wrapped a long C-thunk name onto the next line.

## Archivo modificado / Modified file

- `cmake/VerifyNativeSharpDebugArtifacts.cmake`
  - Actualiza dos expresiones del bloque de validación de símbolos PDB completos.
  - Updates two expressions in the full-PDB symbol validation block.

## Implementación / Implementation

### Filtro de excepción / Exception filter

La regla exige ahora `S_LPROC32` para `DebugFixture.DebugProblem.catch-filter [generated]`. El helper no forma parte de la API ni necesita visibilidad global; su clasificación local describe correctamente su alcance físico.

The rule now requires `S_LPROC32` for `DebugFixture.DebugProblem.catch-filter [generated]`. The helper is not API and needs no global visibility; local procedure classification accurately describes its physical scope.

### Thunk de frontera C / C-boundary thunk

La expresión de `Increment__c_boundary` deja de limitar la búsqueda del nombre a la misma línea que `S_THUNK32`. Continúa exigiendo el tipo de registro y el nombre exacto relevante, pero tolera el salto de línea de formato introducido por `llvm-pdbutil` para nombres largos.

The `Increment__c_boundary` expression no longer requires the name to remain on the same line as `S_THUNK32`. It still requires the record kind and relevant exact name, while accepting `llvm-pdbutil` line wrapping for long symbols.

## Motivo / Rationale

El contrato debe verificar contenido de depuración, alcance y clasificación, no el ancho de presentación de la herramienta de inspección. Exigir `S_LPROC32` para un helper privado fortalece la expectativa correcta. Permitir espacio entre el encabezado del thunk y su nombre elimina sólo la dependencia del formato visual.

The contract must verify debug content, scope, and classification rather than the inspection tool's display width. Requiring `S_LPROC32` for a private helper strengthens the correct expectation. Allowing whitespace between a thunk header and its name removes only a visual-format dependency.

## Validación / Validation

Se inspeccionó el PDB ya producido por el baseline anterior; no se recompiló ni ejecutó el fixture y no se lanzó baseline. La comprobación acumulativa queda para el final.

The PDB previously produced by the prior baseline was inspected. The fixture was neither rebuilt nor executed, and no baseline was started. Cumulative verification remains deferred until the end.

## Commit de referencia / Reference commit

### Título / Title

```text
Corrige la validación PDB de helpers locales y thunks largos / Fix PDB validation for local helpers and long thunks
```

### Descripción / Description

```text
ES:
- exige clasificación local para filtros de excepción internos
- tolera nombres de thunk partidos por llvm-pdbutil
- conserva todas las exigencias de funciones, tipos, variables y rangos

EN:
- require local classification for internal exception filters
- tolerate thunk names wrapped by llvm-pdbutil
- preserve all function, type, variable, and range requirements
```
