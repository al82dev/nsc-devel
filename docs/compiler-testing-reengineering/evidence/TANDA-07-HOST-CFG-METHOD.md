# Evidencia de método — Tanda 07, Host.CFG

## SCOPE LOCK aplicado

La tanda fue exclusivamente documental. Sólo se modificó `docs/compiler-testing-reengineering/**`. Código de
producto, pruebas históricas, builders, unidad C:, remotos y los seis cambios preexistentes permanecieron fuera.

## Universo leído

| Grupo | Archivos | Líneas | Bytes |
|---|---:|---:|---:|
| headers CFG | 10 | 309 | 6,910 |
| sources CFG | 10 | 915 | 27,096 |
| total | 20 | 1,224 | 34,006 |

Archivos: `CfgArena`, `CfgBuilder`, `CfgDumper`, `CfgEdge`, `CfgEdgeKind`, `CfgFunction`, `CfgModule`, `CfgNode`,
`CfgTraversal` y `CfgVerifier`, cada uno en header y source. Todos se leyeron completos.

## Extracción y contraste

1. El manifiesto canónico fijó los 20 `PF-*` y sus huellas.
2. La lectura identificó 11 tipos y 66 funciones o helpers: 77 símbolos.
3. Cada símbolo recibió archivo, línea, responsabilidad, contrato, dependencias, oracle y sección.
4. Se agruparon 26 contratos observables sin registrar manualmente ramas o `if`.
5. Cada contrato recibió ocho matrices parametrizadas: 208 casos diseñados.
6. Una segunda pasada recorrió archivos, símbolos, contratos y casos desde los manifiestos.

## Comprobaciones deterministas

- `20/20` archivos CFG en estado `AUDITADO`.
- `77/77` símbolos con ID único y contrato existente.
- `26/26` contratos con fuente y exactamente ocho matrices.
- `208/208` casos con ID único; 182 `PR` y 26 `NIGHTLY`.
- paridad de 974 filas entre `production-files.csv` y `production-files.json`.
- huellas de los 20 archivos de producción iguales al inventario congelado.
- cero archivos de producto o prueba modificados por la tanda.

## Límites

No se compiló ni ejecutó nada. No se midieron líneas, ramas, mutation score ni rendimiento. `DESIGNED` significa
diseño contractual trazable; no significa prueba implementada.
