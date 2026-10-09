# Evidencia de método — Tanda 07, Host.MIR

## SCOPE LOCK aplicado

La tanda fue exclusivamente documental. Sólo se modificó `docs/compiler-testing-reengineering/**`. Código de
producto, pruebas históricas, builders, unidad C:, remotos y los seis cambios preexistentes permanecieron fuera.

## Universo leído

| Grupo | Archivos | Líneas | Bytes |
|---|---:|---:|---:|
| headers MIR | 11 | 540 | 13,653 |
| sources MIR | 11 | 2,514 | 91,987 |
| total | 22 | 3,054 | 105,640 |

Archivos: `MirArena`, `MirBlock`, `MirBuilder`, `MirDumper`, `MirFunction`, `MirGlobal`, `MirInstruction`,
`MirModule`, `MirOpcode`, `MirValue` y `MirVerifier`, cada uno en header y source. Todos se leyeron completos en
la revisión congelada.

## Extracción y contraste

1. El manifiesto canónico fijó los 22 `PF-*` y sus huellas.
2. La lectura identificó 15 tipos, 123 definiciones calificadas y 14 helpers de namespace: 152 símbolos.
3. Cada símbolo recibió archivo, línea, responsabilidad, contrato, dependencias, oracle y sección.
4. Se agruparon 38 contratos observables sin registrar manualmente ramas o `if`.
5. Cada contrato recibió ocho matrices parametrizadas: 304 casos diseñados.
6. Una segunda pasada recorrió archivos, símbolos, contratos y casos desde los manifiestos.

## Comprobaciones deterministas

- `22/22` archivos MIR en estado `AUDITADO`.
- `152/152` símbolos con ID único y contrato existente.
- `38/38` contratos con fuente y exactamente ocho matrices.
- `304/304` casos con ID único; 266 `PR` y 38 `NIGHTLY`.
- paridad de 974 filas entre `production-files.csv` y `production-files.json`.
- huellas de los 22 archivos de producción iguales al inventario congelado.
- cero archivos de producto o prueba modificados por la tanda.

## Límites

No se compiló ni ejecutó nada. No se midieron líneas, ramas, mutation score ni rendimiento. `DESIGNED` significa
diseño contractual trazable; no significa prueba implementada.
