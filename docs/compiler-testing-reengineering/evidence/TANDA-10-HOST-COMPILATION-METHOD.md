# Evidencia de método — Tanda 10, Host.Compilation

## SCOPE LOCK aplicado

Trabajo exclusivamente documental dentro de `docs/compiler-testing-reengineering/**`. Código, pruebas,
infraestructura, C:, remotos y los seis cambios preexistentes permanecieron protegidos.

## Universo leído e indexado

| Grupo | Archivos | Líneas | Bytes |
|---|---:|---:|---:|
| headers públicos Compilation | 12 | 658 | 21,864 |
| implementaciones Compilation | 12 | 3,597 | 122,383 |
| total | 24 | 4,255 | 144,247 |

Se leyeron completos los 24 archivos. El índice AST identificó definiciones físicas y excluyó lambdas, implícitos,
aliases y forward declarations. Una pasada separada extrajo diagnósticos, límites, defaults, estados de cache,
resolución de paths y transformaciones de grafo.

## Extracción y contraste

1. El manifiesto fijó 24 `PF-*`, tamaños y huellas.
2. Headers y AST identificaron 160 símbolos: 41 tipos definidos y 119 funciones físicas.
3. Se diseñaron 52 contratos observables, no ramas internas ni conteos de helpers.
4. Cada contrato recibió ocho matrices: 416 casos, 364 `PR` y 52 `NIGHTLY`.
5. La segunda pasada contrastó archivos, IDs, contratos, casos, diagnósticos, oráculos y hashes.
6. La recursión vigente se conservó como contradicción; no se modificó el producto.

## Límites

No hubo build, ejecución, baseline, cobertura, lectura mutable de proyectos reales, instalación, despliegue ni
modificación de producto. El indexado AST sólo analizó fuentes. `DESIGNED` no significa implementado.
