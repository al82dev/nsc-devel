# Segunda auditoría independiente — Host.Dominance

## Método

La segunda pasada comenzó en los 18 registros `PF-*`. Recalculó ownership, CHK, intersección, materialización,
numeración, frontier, consultas, dump y los oracles de sets del verifier. Después contrastó manifiestos, símbolos,
contratos y casos sin usar la narrativa como fuente primaria.

## Resultado cuantitativo

| Control | Valor |
|---|---:|
| archivos | 18 |
| líneas | 1,512 |
| bytes | 48,578 |
| tipos | 10 |
| funciones y helpers | 71 |
| símbolos | 81 |
| contratos | 32 |
| casos diseñados | 256 |
| casos PR | 224 |
| casos NIGHTLY | 32 |
| omitidos o desconocidos | 0 |

## Gates

- 18 archivos con huella, estado, evidencia y sección;
- 81 símbolos con archivo, línea y contrato;
- 32 contratos con fuente y exactamente ocho matrices;
- 256 casos con IDs únicos y referencias válidas;
- paridad de 974 filas CSV/JSON y hashes fuente iguales al congelado;
- cero cambios en producción, pruebas, builders, C: o remotos;
- seis archivos preexistentes conservados byte por byte.

## Hallazgos conservados

1. El builder aplica CHK sobre el RPO ya calculado por CFG.
2. `DominanceBuilder::build` no limpia un error previo; una instancia nueva es el uso vigente.
3. Numeración y dump del árbol usan stacks explícitas.
4. Frontier se deduplica por set y se ordena por RPO mediante inserción.
5. El verifier usa conjuntos de dominadores, un algoritmo distinto al builder.
6. `addFunction` y `addNode` admiten estados parciales que el verifier rechaza.
7. Las consultas públicas de dominancia dependen de intervalos pre/post materializados correctamente.

Son observaciones estáticas, no defectos corregidos.

## Dictamen

`Host.Dominance` queda `AUDITADO` y `RECONSTRUIBLE` para la suite nueva. No se afirma ejecución, cobertura ni
ausencia de defectos de producto.
