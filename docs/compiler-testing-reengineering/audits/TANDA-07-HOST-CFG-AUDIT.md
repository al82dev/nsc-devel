# Segunda auditoría independiente — Host.CFG

## Método

La segunda pasada comenzó en los 20 registros `PF-*`, no en la narrativa. Reconstruyó por separado el modelo,
la derivación MIR, el DFS iterativo, el dump y cada gate del verifier. Después contrastó archivos, símbolos,
contratos y matrices desde los manifiestos mecánicos.

## Resultado cuantitativo

| Control | Valor |
|---|---:|
| archivos | 20 |
| líneas | 1,224 |
| bytes | 34,006 |
| tipos | 11 |
| funciones y helpers | 66 |
| símbolos | 77 |
| contratos | 26 |
| casos diseñados | 208 |
| casos PR | 182 |
| casos NIGHTLY | 26 |
| omitidos o desconocidos | 0 |

## Gates

- cada archivo tiene huella, tamaño, líneas, estado, evidencia y sección;
- cada símbolo tiene archivo, línea, responsabilidad y contrato;
- cada contrato tiene entradas, invariantes, errores, dependencias y oracle;
- cada contrato posee ocho matrices y cada caso referencia un contrato existente;
- CSV y JSON conservan el mismo inventario de 974 archivos;
- no se tocó código del compilador, tests, builders, C: o remotos;
- las huellas de los seis cambios preexistentes permanecen iguales.

## Hallazgos conservados

1. Traversal usa stack explícita y no contiene recursión.
2. `CfgBuilder::build` no limpia el error de una instancia reutilizada.
3. Las colecciones aceptan estados parciales que sólo cierra `CfgVerifier`.
4. Predecessors y successors deduplican con búsqueda lineal en vector.
5. El verifier exige registro exacto de cada edge en ambos endpoints.
6. Exit y reachable exit son conceptos distintos y se conservan en orden de nodos.

Son observaciones de la revisión congelada, no defectos corregidos ni autorización de implementación.

## Dictamen

`Host.CFG` queda `AUDITADO` y `RECONSTRUIBLE` para diseñar la suite nueva. No se afirma ejecución, cobertura ni
ausencia de defectos del producto.
