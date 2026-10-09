# Evidencia de método — Tanda 07, Host.HIR

## SCOPE LOCK aplicado

La tanda fue exclusivamente documental. Sólo se modificó `docs/compiler-testing-reengineering/**`. Código de
producto, pruebas históricas, builders, unidad C:, remotos y los seis cambios preexistentes permanecieron fuera.

## Universo leído

| Grupo | Archivos | Líneas | Bytes |
|---|---:|---:|---:|
| headers HIR | 6 | 287 | 7,525 |
| sources HIR | 6 | 1,774 | 67,444 |
| total | 12 | 2,061 | 74,969 |

Archivos: `HirArena`, `HirBuilder`, `HirDumper`, `HirNode`, `HirNodeKind` y `HirVerifier`, cada uno en header y
source. Todos se leyeron completos en la revisión congelada.

## Extracción y contraste

1. El manifiesto canónico fijó los 12 `PF-*` y sus huellas.
2. La lectura identificó seis tipos y 82 definiciones físicas: 88 símbolos.
3. Cada símbolo recibió archivo, línea, responsabilidad, contrato, dependencias, oracle y sección.
4. Se agruparon 29 contratos observables sin registrar manualmente ramas o `if`.
5. Cada contrato recibió ocho matrices parametrizadas: 232 casos diseñados.
6. Una segunda pasada recorrió archivos, símbolos, contratos y casos desde los manifiestos.

## Comprobaciones deterministas

- `12/12` archivos HIR en estado `AUDITADO`.
- `88/88` símbolos con ID único y contrato existente.
- `29/29` contratos con fuente y exactamente ocho matrices.
- `232/232` casos con ID único; 203 `PR` y 29 `NIGHTLY`.
- paridad de 974 filas entre `production-files.csv` y `production-files.json`.
- huellas de los 12 archivos de producción iguales al inventario congelado.
- cero archivos de producto o prueba modificados por la tanda.

## Límites

No se compiló ni ejecutó nada. No se midieron líneas, ramas, mutation score ni rendimiento. `DESIGNED` significa
diseño contractual trazable; no significa prueba implementada.
