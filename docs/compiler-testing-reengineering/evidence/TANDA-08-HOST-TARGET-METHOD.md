# Evidencia de método — Tanda 08, Host.Target

## SCOPE LOCK aplicado

Trabajo exclusivamente documental dentro de `docs/compiler-testing-reengineering/**`. Código, pruebas,
infraestructura, C:, remotos y los seis cambios preexistentes permanecieron protegidos.

## Universo leído

| Grupo | Archivos | Líneas | Bytes |
|---|---:|---:|---:|
| headers Target | 2 | 200 | 6,157 |
| sources Target | 2 | 581 | 20,780 |
| total | 4 | 781 | 26,937 |

Se leyeron completos `TargetDataLayout` y `TargetInfo`, headers y sources. Se recorrieron todas las tablas, rutas
de overflow, opciones agregadas, estados bit-field, niveles CPU y parsers de nombres.

## Extracción y contraste

1. El manifiesto fijó 4 `PF-*` y sus huellas.
2. Se identificaron 13 tipos y 33 funciones: 46 símbolos.
3. Se diseñaron 30 contratos observables.
4. Cada contrato recibió ocho matrices: 240 casos, 210 `PR` y 30 `NIGHTLY`.
5. La segunda pasada recalculó layouts y tablas sin usar el documento narrativo como fuente.

## Límites

No hubo build, ejecución, baseline, cobertura, consulta a LLVM ni modificación de producto. `DESIGNED` no
significa implementado.
