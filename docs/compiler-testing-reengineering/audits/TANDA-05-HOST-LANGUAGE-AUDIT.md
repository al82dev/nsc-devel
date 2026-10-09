# Segunda auditoría independiente — Host.Language

## Método

La segunda pasada no siguió el orden narrativo de `HOST-LANGUAGE.md`. Partió de cuatro inventarios físicos:
los 42 archivos, 225 declaraciones de tipos, 280 definiciones de source y 20 miembros/constantes inline. Luego
reconstruyó cada tabla, state machine, parser, registry y writer desde sus entradas y salidas y lo contrastó
contra contratos y casos. Los 21 grupos de estado interno se verificaron al recorrer sus productores y
consumidores.

## Resultado cuantitativo obtenido

| Control | Valor |
|---|---:|
| archivos | 42 |
| líneas | 7,323 |
| bytes | 263,231 |
| tipos de header | 222 |
| tipos locales de source | 3 |
| definiciones físicas de source | 280 |
| símbolos | 546 |
| contratos | 21 |
| casos diseñados | 286 |
| casos PR | 277 |
| casos NIGHTLY | 9 |
| archivos/símbolos/contratos omitidos | 0 |
| IDs duplicados o referencias inválidas | 0 |
| diferencias de hash/tamaño/líneas | 0 |
| diferencias CSV/JSON o Markdown/CSV | 0 |

## Gates reproducidos

- 546/546 símbolos tienen archivo, documento, sección y exactamente un contrato propietario.
- 21/21 contratos tienen símbolos productores y al menos un caso positivo, negativo o de borde aplicable.
- 286/286 casos forman la secuencia `TL-LANG-001..286` y referencian contratos existentes.
- 225/225 tipos y 280/280 definiciones físicas aparecen en el inventario semántico.
- 42/42 archivos conservan las huellas, tamaños y líneas congelados.
- 42/42 filas tienen trazabilidad bidireccional archivo → símbolos → contrato → sección.
- Los inventarios CSV y JSON de producción describen los mismos 974 archivos y estados.
- El plan Markdown y `test-case-design.csv` contienen los mismos 286 IDs, contratos, objetivos y frecuencias.
- Los cambios permanecen dentro de `docs/compiler-testing-reengineering/**`; los seis cambios preexistentes
  siguen separados.

## Riesgos y contradicciones conservados

Los gates históricos ligados a 3.x, los conceptos `unsafe` y las etiquetas de edición se documentan como
implementación vigente frente a decisiones posteriores del usuario. No se resolvieron silenciosamente.

`moduleGraphResolvable` usa DFS recursivo sin límite explícito. La discrepancia con la política general contra
recursión queda registrada y no se transforma en una corrección fuera de alcance.

Los lookups de `ExpressionErgonomics`, `Language3Surface` y `SmartPointerSemantics` no comparten una política
segura para enums externos. El plan caracteriza cada conducta actual; no inventa una normalización global.

`ModuleLifetimeRegistry` incorpora `ModuleUnloadPolicy` al fingerprint pero no ramifica conducta por policy;
`incrementalRebuildSet` no incorpora por sí solo módulos eliminados; ambos hechos permanecen visibles.

## Dictamen

`Host.Language` es `AUDITADO` y `RECONSTRUIBLE` para la revisión congelada. El dictamen significa suficiencia
documental para reconstruir el comportamiento vigente. No afirma que los 286 casos estén implementados, que
hayan sido ejecutados ni que exista coverage instrumental.
