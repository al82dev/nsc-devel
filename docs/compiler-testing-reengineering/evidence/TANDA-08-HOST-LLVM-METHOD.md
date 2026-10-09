# Evidencia de método — Tanda 08, Host.LLVM

## SCOPE LOCK aplicado

Trabajo exclusivamente documental dentro de `docs/compiler-testing-reengineering/**`. Código, pruebas,
infraestructura, C:, remotos y los seis cambios preexistentes permanecieron protegidos.

## Universo leído e indexado

| Grupo | Archivos | Líneas | Bytes |
|---|---:|---:|---:|
| headers públicos LLVM | 20 | 1,229 | 37,189 |
| sources y soporte LLVM | 20 | 23,886 | 1,232,136 |
| total | 40 | 25,115 | 1,269,325 |

Se leyeron completos los archivos pequeños, `LlvmCApi.h` y `LlvmLowerer.cpp`. El lowerer OOP de 17,079 líneas
se recorrió por índice AST, firmas, rangos funcionales, casos de dispatch, diagnósticos y lectura dirigida de cada
subsistema. El índice excluyó lambdas e implícitos y conservó definiciones físicas; para LLVM-C incluyó las 230
declaraciones externas porque no existe definición local.

## Extracción y contraste

1. El manifiesto fijó 40 `PF-*` y sus huellas.
2. El AST identificó 802 símbolos: 23 classes, 22 structs, 33 enums, 15 constructores, 5 destructores, 474
   funciones definidas y 230 funciones externas LLVM-C.
3. Se diseñaron 64 contratos observables por frontera estable, no por branch interna.
4. Cada contrato recibió ocho matrices: 512 casos, 448 `PR` y 64 `NIGHTLY`.
5. La segunda pasada partió otra vez de los 40 archivos y comprobó manifiestos, IDs, contratos, casos y oráculos.

## Límites

No hubo build, ejecución, baseline, cobertura, consulta de red, instalación LLVM, despliegue ni modificación de
producto. El indexado AST sólo analizó fuentes; no produjo binarios. `DESIGNED` no significa implementado.
