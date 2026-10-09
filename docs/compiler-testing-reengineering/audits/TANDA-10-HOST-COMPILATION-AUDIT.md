# Segunda auditoría independiente — Host.Compilation

## Método

La segunda pasada partió de los 24 registros `PF-*`. Recontó archivos, líneas, bytes, tipos y funciones; reagrupó
glob, identidad, parciales, XML, proyecto, fuentes, SDK y grafo; después contrastó documentos, contratos y casos.

## Resultado cuantitativo

| Control | Valor |
|---|---:|
| archivos | 24 |
| líneas | 4,255 |
| bytes | 144,247 |
| tipos definidos | 41 |
| funciones definidas | 119 |
| símbolos | 160 |
| contratos | 52 |
| casos diseñados | 416 |
| casos PR | 364 |
| casos NIGHTLY | 52 |
| omitidos o desconocidos | 0 |

## Gates

- 24 archivos con huella, estado, evidencia y sección;
- 160 símbolos físicos con archivo, línea y contrato;
- 52 contratos con fuente y exactamente ocho matrices;
- 416 casos con IDs únicos y referencias válidas;
- todos los diagnósticos y límites de loaders/parsers asociados a contrato;
- hashes fuente iguales al manifiesto congelado;
- cero cambios en producto, pruebas, builders, C: o remotos;
- seis archivos preexistentes conservados byte por byte.

## Hallazgos conservados

1. `<Language>` se consume como compatibilidad y no controla aceptación sintáctica.
2. `NS_LIB_ROOT` sólo actúa como fallback y no sustituye un artefacto local existente.
3. El grafo publica orden dependency-first con root al final y ownership único de fuentes.
4. Los nodos NSMETA son proyectos sintéticos con identidad de módulo propia.
5. `.nsproj` y `.nssdk` tienen límite de 4 MiB; XML tiene profundidad máxima 128.
6. Globbing, DFS de grafo y XML conservan recursión vigente; sólo XML está acotado.
7. Identidades de tipo producen fragmentos reversibles y quoting LLVM explícito.
8. Caches, colecciones y rewrites del grafo tienen orden/locks observables.

Son observaciones estáticas, no defectos corregidos.

## Dictamen

`Host.Compilation` queda `AUDITADO` y `RECONSTRUIBLE`. No se afirma ejecución, cobertura ni ausencia de defectos de
producto.
