# Evidencia de método — Tanda 09, Host.Linker

## SCOPE LOCK aplicado

Trabajo exclusivamente documental dentro de `docs/compiler-testing-reengineering/**`. Código, pruebas,
infraestructura, C:, remotos y los seis cambios preexistentes permanecieron protegidos.

## Universo leído e indexado

| Grupo | Archivos | Líneas | Bytes |
|---|---:|---:|---:|
| headers públicos Linker | 8 | 425 | 14,174 |
| implementaciones Linker | 8 | 3,937 | 146,345 |
| total | 16 | 4,362 | 160,519 |

Se leyeron completos los headers. Las implementaciones se recorrieron mediante índice AST, firmas, diagnósticos,
constantes, variables de entorno y lectura dirigida de parsers, ejecución de proceso, builders, caches, imports y
runtime. El índice incluyó tipos locales y excluyó lambdas e implícitos.

## Extracción y contraste

1. El manifiesto fijó 16 `PF-*`, tamaños y huellas.
2. AST y headers identificaron 132 símbolos: 17 tipos definidos y 115 funciones físicas.
3. Se diseñaron 40 contratos observables, no ramas internas.
4. Cada contrato recibió ocho matrices: 320 casos, 280 `PR` y 40 `NIGHTLY`.
5. La segunda pasada contrastó archivos, IDs, contratos, casos, oráculos y hashes.

## Límites

No hubo build, ejecución, baseline, cobertura, procesos de enlace, instalación, despliegue ni modificación de
producto. El indexado AST sólo analizó fuentes. `DESIGNED` no significa implementado.
