# Evidencia de método — Tanda 10, Host.Driver

## SCOPE LOCK aplicado

Trabajo exclusivamente documental dentro de `docs/compiler-testing-reengineering/**`. Código, pruebas,
infraestructura, C:, remotos y los seis cambios preexistentes permanecieron protegidos.

## Universo leído e indexado

| Grupo | Archivos | Líneas | Bytes |
|---|---:|---:|---:|
| headers públicos Driver | 26 | 901 | 27,846 |
| implementaciones Driver | 25 | 14,422 | 647,567 |
| total | 51 | 15,323 | 675,413 |

Se leyeron completos los 51 archivos. El índice AST identificó definiciones físicas y deduplicó instanciaciones de
templates por archivo, línea, clase y nombre. Se excluyeron lambdas, implícitos, aliases y forward declarations.

## Extracción y contraste

1. El manifiesto fijó 51 `PF-*`, tamaños y huellas.
2. Headers y AST identificaron 387 símbolos: 61 tipos definidos y 326 funciones físicas.
3. Se separaron 77 contratos observables de CLI, concurrencia, productos, metadata, paquetes e instalación.
4. Cada contrato recibió ocho matrices: 616 casos, 539 `PR` y 77 `NIGHTLY`.
5. La segunda pasada contrastó archivos, IDs, contratos, casos, diagnósticos, transacciones y hashes.
6. Recursión y artefactos versionados vigentes se conservaron como contradicciones; no se modificó producto.

## Límites

No hubo build, ejecución, baseline, cobertura, instalación, despliegue ni modificación de producto. El indexado AST
sólo analizó fuentes. `DESIGNED` no significa implementado.
