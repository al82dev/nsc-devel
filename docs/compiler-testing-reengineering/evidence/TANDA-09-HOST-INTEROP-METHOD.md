# Evidencia de método — Tanda 09, Host.Interop

## SCOPE LOCK aplicado

Trabajo exclusivamente documental dentro de `docs/compiler-testing-reengineering/**`. Código, pruebas,
infraestructura, C:, remotos y los seis cambios preexistentes permanecieron protegidos.

## Universo leído e indexado

| Grupo | Archivos | Líneas | Bytes |
|---|---:|---:|---:|
| headers públicos Interop | 6 | 759 | 35,361 |
| implementaciones Interop | 6 | 7,664 | 352,455 |
| total | 12 | 8,423 | 387,816 |

Se leyeron completos los encabezados y las cinco implementaciones menores. `LibraryMetadata.cpp`, de 5,181
líneas, se recorrió mediante índice AST, firmas, rangos de construcción/codificación/decodificación/escritura/
verificación, compuertas de minor y extracción de todos los diagnósticos. El índice excluyó lambdas e implícitos.

## Extracción y contraste

1. El manifiesto fijó 12 `PF-*`, sus tamaños y huellas.
2. El AST y los headers identificaron 178 símbolos: 41 tipos definidos y 137 funciones físicas.
3. Se diseñaron 48 contratos observables por frontera estable, no por branch interna.
4. Cada contrato recibió ocho matrices: 384 casos, 336 `PR` y 48 `NIGHTLY`.
5. La segunda pasada partió otra vez de los 12 archivos y comprobó IDs, contratos, casos, oráculos y hashes.

## Límites

No hubo build, ejecución, baseline, cobertura, consulta de red, instalación, despliegue ni modificación de
producto. El indexado AST sólo analizó fuentes; no produjo binarios. `DESIGNED` no significa implementado.
