# Evidencia de método — Tanda 07, Host.SSA

## SCOPE LOCK aplicado

La tanda fue exclusivamente documental. Sólo se modificó `docs/compiler-testing-reengineering/**`. Código de
producto, pruebas históricas, builders, unidad C:, remotos y los seis cambios preexistentes permanecieron fuera.

## Universo leído

| Grupo | Archivos | Líneas | Bytes |
|---|---:|---:|---:|
| headers SSA | 7 | 457 | 12,973 |
| sources SSA | 7 | 2,188 | 70,788 |
| total | 14 | 2,645 | 83,761 |

Archivos: `SsaArena`, `SsaBuilder`, `SsaDumper`, `SsaFunction`, `SsaModule`, `SsaPromotability` y `SsaVerifier`,
cada uno en header y source. Todos se leyeron completos, incluidos los 973 renglones del builder y los 714 del
verifier.

## Extracción y contraste

1. El manifiesto canónico fijó los 14 `PF-*` y sus huellas.
2. La lectura identificó 10 tipos y 92 funciones o helpers: 102 símbolos.
3. Cada símbolo recibió archivo, línea, responsabilidad, contrato, dependencias, oracle y sección.
4. Se agruparon 42 contratos observables sin registrar manualmente ramas o `if`.
5. Cada contrato recibió ocho matrices parametrizadas: 336 casos diseñados.
6. La segunda pasada partió nuevamente de los 14 registros del manifiesto y recalculó cada gate.

## Comprobaciones deterministas

- `14/14` archivos SSA en estado `AUDITADO`.
- `102/102` símbolos con ID único y contrato existente.
- `42/42` contratos con fuente y exactamente ocho matrices.
- `336/336` casos con ID único; 294 `PR` y 42 `NIGHTLY`.
- paridad de 974 filas entre `production-files.csv` y `production-files.json`.
- huellas de los 14 archivos de producción iguales al inventario congelado.
- cero archivos de producto o prueba modificados por la tanda.

## Límites

No se compiló ni ejecutó nada. No se midieron líneas, ramas, mutation score ni rendimiento. `DESIGNED` significa
diseño contractual trazable; no significa prueba implementada.
