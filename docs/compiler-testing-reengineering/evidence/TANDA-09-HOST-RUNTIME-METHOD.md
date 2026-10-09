# Evidencia de método — Tanda 09, Host.Runtime

## SCOPE LOCK aplicado

Trabajo exclusivamente documental dentro de `docs/compiler-testing-reengineering/**`. Código, pruebas,
infraestructura, C:, remotos y los seis cambios preexistentes permanecieron protegidos.

## Universo leído e indexado

| Grupo | Archivos | Líneas | Bytes |
|---|---:|---:|---:|
| headers públicos Runtime | 3 | 221 | 13,812 |
| implementaciones Runtime | 3 | 6,970 | 370,700 |
| total | 6 | 7,191 | 384,512 |

Se leyeron completos los headers y las dos implementaciones pequeñas. `WindowsRuntimeBuilder.cpp` se recorrió con
índice AST de definiciones físicas, firmas, comentarios contractuales, diagnósticos, exports/imports y lectura por
familias funcionales. El índice excluyó lambdas, declaraciones implícitas y el forward declaration de `LlvmModule`.

## Extracción y contraste

1. El manifiesto fijó seis `PF-*`, tamaños y huellas.
2. Headers y AST identificaron 129 símbolos: siete tipos definidos y 122 funciones físicas.
3. Se contrastaron las 99 entradas del catálogo ABI y sus tres particiones de imagen.
4. Se diseñaron 48 contratos observables, no ramas internas ni número de helpers.
5. Cada contrato recibió ocho matrices: 384 casos, 336 `PR` y 48 `NIGHTLY`.
6. La segunda pasada contrastó archivos, IDs, contratos, casos, oráculos y hashes.

## Límites

No hubo build, ejecución, baseline, cobertura, proceso Win32, instalación, despliegue ni modificación de producto.
El indexado AST sólo analizó fuentes. `DESIGNED` no significa implementado.
