# Evidencia de método — Tanda 10, Host.nsc

## SCOPE LOCK aplicado

Auditoría estática exclusivamente documental de `src/nsc/main.cpp`. No se modificó ni ejecutó producto.

## Universo leído e indexado

| Archivos | Líneas | Bytes | Símbolos |
|---:|---:|---:|---:|
| 1 | 551 | 24,169 | 18 |

La extracción AST Windows encontró 17 roots: una clase y 16 funciones-like. La segunda pasada física añadió el
entry point `main` de la rama no Windows, excluida por preprocesamiento en el host, para inventariar ambos caminos
versionados en el archivo.

## Extracción y contraste

1. Se leyó el archivo completo con números de línea.
2. `clang-query` indexó declaraciones activas sin build ni artefactos.
3. Se contrastaron manualmente las ramas `_WIN32`/no Windows y se conservó `main`.
4. Se separaron contratos de helpers, proceso, tracing y observabilidad.
5. Se diseñaron 20 contratos y 160 casos; cada contrato contiene ocho casos.
6. La segunda auditoría recalcula IDs, referencias, hashes y navegación.

## Límites

No hubo build, ejecución, baseline, cobertura, child process, escritura de logs reales ni despliegue.
