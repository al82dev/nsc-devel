# Segunda auditoría independiente — Host.Parser

## Método

La segunda pasada no reutilizó el orden narrativo del documento de componente. Partió de tres inventarios
independientes: las 147 definiciones `Parser::`, los 20 IDs diagnósticos y los 99 kinds AST. Cada elemento se
contrastó contra símbolos, contratos y casos. Después se recorrieron las rutas de recuperación y los lowerings
para comprobar progreso, evaluación única, rangos y estado restaurado.

## Resultado cuantitativo obtenido

| Control | Valor |
|---|---:|
| archivos | 2 |
| líneas | 14,462 |
| bytes | 614,860 |
| definiciones miembro | 147 |
| símbolos | 174 |
| DiagnosticId distintos usados | 20 |
| AstNodeKind distintos usados | 99 |
| contratos | 42 |
| casos diseñados | 536 |
| casos PR | 525 |
| casos NIGHTLY | 11 |
| archivos/símbolos/contratos omitidos | 0 |
| IDs duplicados o referencias inválidas | 0 |
| diferencias de hash/tamaño/líneas | 0 |
| diferencias CSV/JSON o Markdown/CSV | 0 |

## Gates reproducidos

- 174/174 símbolos Parser tienen documento, sección y contrato.
- 42/42 contratos tienen símbolos productores y casos.
- 536/536 casos forman la secuencia `TP-PAR-001..536` y referencian contratos existentes.
- 147/147 definiciones miembro tienen declaración e inventario.
- Ambos archivos conservan las huellas congeladas.
- Los cambios del trabajo se mantienen en `docs/compiler-testing-reengineering/**`; los seis cambios
  preexistentes siguen separados.

## Riesgos expresamente conservados

La implementación usa recursión en clonación/propagación de AST, tipos, deconstruction, patrones,
right-associative expressions, unary e interpolación. Parser no impone una profundidad central. Esto queda
caracterizado por `CP-PAR-042` y casos NIGHTLY; no se ocultó ni se modificó en una fase documental.

También existen nombres históricos de etapas/`Language3` en comentarios y diagnósticos. La auditoría no los
convierte en una barrera de versión que el código no demuestre, ni los reescribe fuera de alcance.

## Dictamen

`Host.Parser` es `AUDITADO` y `RECONSTRUIBLE` para la revisión congelada. El dictamen significa que su
comportamiento vigente puede reconstruirse a partir de símbolos, contratos y oráculos documentados. No afirma
que las 536 pruebas estén implementadas, que hayan sido ejecutadas ni que exista coverage instrumental.
