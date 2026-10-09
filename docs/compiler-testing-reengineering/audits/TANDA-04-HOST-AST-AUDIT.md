# Segunda auditoría independiente — Host.AST

## Método

La segunda pasada no siguió el orden narrativo del documento. Partió de cuatro inventarios independientes:
las 61 definiciones `AstNode::`, las 61 definiciones `TypeSyntax::`, los 100 enumeradores `AstNodeKind` y los
diez archivos congelados. Después reconstruyó los canales de estado desde sus campos y oráculos y los
contrastó contra símbolos, contratos y casos.

## Resultado cuantitativo obtenido

| Control | Valor |
|---|---:|
| archivos | 10 |
| líneas | 1,372 |
| bytes | 58,082 |
| definiciones `AstNode::` | 61 |
| definiciones `TypeSyntax::` | 61 |
| valores `AstNodeKind` | 100 |
| símbolos | 155 |
| contratos | 20 |
| casos diseñados | 230 |
| casos PR | 217 |
| casos NIGHTLY | 13 |
| archivos/símbolos/contratos omitidos | 0 |
| IDs duplicados o referencias inválidas | 0 |
| diferencias de hash/tamaño/líneas | 0 |
| diferencias CSV/JSON o Markdown/CSV | 0 |

## Gates reproducidos

- 155/155 símbolos AST tienen documento, sección y al menos un contrato.
- 20/20 contratos tienen símbolos productores y casos.
- 230/230 casos forman la secuencia `TA-AST-001..230` y referencian contratos existentes.
- 61/61 definiciones `AstNode::` y 61/61 definiciones `TypeSyntax::` están declaradas e inventariadas.
- 100/100 valores `AstNodeKind` tienen un nombre conocido y los valores externos caen en el fallback.
- Los diez archivos conservan las huellas, tamaños y líneas congelados.
- Los cambios se mantienen en `docs/compiler-testing-reengineering/**`; los seis cambios preexistentes siguen
  separados.

## Riesgos expresamente conservados

`AstArena::make` construye con `new` antes de registrar en el vector. Si el registro lanza después de que la
construcción terminó, la implementación vigente no libera ese objeto. El riesgo se documenta; no se corrige en
esta fase.

`AstNode` y `TypeSyntax` son modelos permisivos: aceptan nulls, duplicados, ciclos y combinaciones
semánticamente incoherentes. Además, `clearTypeArguments` no borra los nombres tuple. El plan prueba el
contrato existente sin convertir estos contenedores en validadores.

`AstDumper` es una vista diagnóstica parcial y recursiva. Omite varios canales, no detecta ciclos y no limita
profundidad. Los casos de omisión y adversariales caracterizan el comportamiento sin presentarlo como
serialización reconstructiva.

## Dictamen

`Host.AST` es `AUDITADO` y `RECONSTRUIBLE` para la revisión congelada. El dictamen significa suficiencia
documental para reconstruir el comportamiento vigente. No afirma que los 230 casos estén implementados, que
hayan sido ejecutados ni que exista coverage instrumental.
