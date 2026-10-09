# Evidencia de método — Tanda 10, Host.LanguageService

## SCOPE LOCK aplicado

Se realizó auditoría estática exclusivamente documental. No se modificó ni ejecutó producto, pruebas o build.

## Universo leído e indexado

| Grupo | Archivos | Líneas | Bytes | Símbolos |
|---|---:|---:|---:|---:|
| Headers | 2 | 284 | 7,774 | 61 |
| Implementaciones | 2 | 1,330 | 56,113 | 65 |
| Total | 4 | 1,614 | 63,887 | 126 |

Distribución de símbolos: 4 clases, 21 structs, 3 enums, 28 funciones, 60 métodos, 8 constructores y 2
destructores. La extracción excluyó declaraciones implícitas y call operators de lambdas.

## Extracción y contraste

1. Se leyeron íntegramente los cuatro archivos; `LanguageService.cpp` se releyó por rangos numerados para evitar
   truncamiento del medio.
2. `clang-query` se usó como parser AST sin build ni artefactos; los 126 roots se contrastaron contra fuentes.
3. Se distinguieron declaraciones y definiciones físicas, implementaciones privadas y tipos de transporte.
4. Los símbolos se agruparon por contratos observables, no por ramas internas.
5. Se diseñaron 42 contratos y 336 casos; cada contrato conserva exactamente ocho casos.
6. La segunda pasada verifica archivos, símbolos, referencias, IDs, hashes y navegación documental.

## Límites

No hubo build, prueba, baseline, cobertura, instalación ni despliegue. `DESIGNED` no significa implementado.
