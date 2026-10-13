# NS304-T10-F05 — `?.`/`?[]` a la izquierda con evaluación condicional del RHS

- **Estado**: PARCIAL (`obj?.Field = value` y `?[]` nativo) / residual (indexador de usuario)
- **Rama**: development

## Resultado

- **Sema** (`analyzeAssignment`): `obj?.Field = value` ya no se rechaza como "not assignable".
  `activeNullConditionalWrite_` suprime el chequeo de resultado nullable del READ, admitiendo
  campos de tipo valor (`int`), y se conserva la propiedad `ownsStorageDestination`.
- **HIR** (`ProjectOopHirModel`): nuevo nodo `NullConditionalFieldStore` (receiver + value) en la
  rama de asignación.
- **MIR/LLVM**: `NullConditionalFieldStore` evalúa el receptor una vez, ramifica de inmediato y
  sólo baja el RHS en la rama no nula. Un PHI devuelve cero/null en la rama nula y el valor
  efectivamente almacenado en la rama no nula.
- Prueba `TL-LANG-340`: dos receptores evaluados una vez, un solo RHS, resultado nulo `0` y
  resultado/store no nulo `42` → **PASS** (2026-10-13).

## Residual

- **Indexador de usuario** `obj?[i]`: fuera de la base de array nativo.
