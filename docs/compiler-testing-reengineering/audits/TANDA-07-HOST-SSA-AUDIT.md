# Segunda auditoría independiente — Host.SSA

## Método

La segunda pasada comenzó en los 14 registros `PF-*`, no en la narrativa. Reconstruyó separadamente ownership,
promotabilidad, clasificación de valores, exclusión de bloques, cierre IDF, renombrado, clonación, dump y cada
recomputación del verifier. Después contrastó archivos, símbolos, contratos y matrices desde los manifiestos.

## Resultado cuantitativo

| Control | Valor |
|---|---:|
| archivos | 14 |
| líneas | 2,645 |
| bytes | 83,761 |
| tipos | 10 |
| funciones y helpers | 92 |
| símbolos | 102 |
| contratos | 42 |
| casos diseñados | 336 |
| casos PR | 294 |
| casos NIGHTLY | 42 |
| omitidos o desconocidos | 0 |

## Gates

- cada archivo tiene huella, tamaño, líneas, estado, evidencia y sección;
- cada símbolo tiene archivo, línea, responsabilidad y contrato;
- cada contrato tiene entradas, invariantes, errores, dependencias y oracle;
- cada contrato posee ocho matrices y cada caso referencia un contrato existente;
- CSV y JSON conservan el mismo inventario de 974 archivos;
- los 14 hashes de Host.SSA coinciden con la revisión congelada;
- no se tocó código del compilador, tests, builders, C: o remotos;
- las huellas de los seis cambios preexistentes permanecen iguales.

## Hallazgos conservados

1. La promoción se limita a storage local de kind variable o parameter sin `AddressOfSymbol`.
2. Los valores virtuales multi-definidos se ordenan por ID con inserción cuadrática en el peor caso.
3. La colocación IDF y el renombrado del árbol dominador son iterativos y no usan recursión.
4. Los bloques inalcanzables quedan excluidos y su conteo permanece observable.
5. El verifier recalcula la promotabilidad y la colocación de PHI sin confiar en el builder.
6. El verifier contrasta nodos exactos para PHI de símbolos, pero sólo cardinalidad agregada para PHI virtuales.
7. `SsaModule::addFunction` y `SsaFunction::mapBlock` permiten estados parciales que el verifier debe rechazar.
8. Builder y verifier limpian el error al iniciar la operación y conservan el primer diagnóstico posterior.

Son observaciones de la revisión congelada, no defectos corregidos ni autorización de implementación.

## Dictamen

`Host.SSA` queda `AUDITADO` y `RECONSTRUIBLE` para diseñar la suite nueva. No se afirma ejecución, cobertura ni
ausencia de defectos del producto.
