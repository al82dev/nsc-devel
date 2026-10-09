# Segunda auditoría independiente — Host.HIR

## Método

La segunda pasada comenzó en los 12 registros `PF-*`, no en la narrativa. Volvió a contar tipos y definiciones,
reconstruyó el flujo AST/Sema → Builder → Node/Arena → Verifier/Dumper y contrastó cada símbolo con contrato,
documento y matriz de casos.

## Resultado cuantitativo

| Control | Valor |
|---|---:|
| archivos | 12 |
| líneas | 2,061 |
| bytes | 74,969 |
| tipos | 6 |
| definiciones físicas | 82 |
| símbolos | 88 |
| contratos | 29 |
| casos diseñados | 232 |
| casos PR | 203 |
| casos NIGHTLY | 29 |
| omitidos o desconocidos | 0 |

## Gates

- cada archivo tiene huella, tamaño, líneas, estado, evidencia y sección;
- cada símbolo tiene archivo, línea, responsabilidad y contrato;
- cada contrato tiene productores, entradas, invariantes, errores, dependencies y oracle;
- cada contrato posee ocho matrices y cada caso referencia un contrato existente;
- CSV y JSON conservan el mismo inventario de 974 archivos;
- no se tocó código del compilador, tests, builders, C: o remotos;
- las huellas de los seis cambios preexistentes permanecen iguales.

## Hallazgos conservados

1. Builder, verifier y dumper poseen recorridos recursivos y no detectan ciclos.
2. `HirBuilder::build` no limpia el error de una instancia reutilizada.
3. `static_assert` se representa como block vacío después de Sema.
4. El verifier valida relaciones HIR, pero deliberadamente no repite toda la semántica de operadores.
5. Los setters permiten estados incompletos hasta el gate del verifier.

Son observaciones de la revisión congelada, no defectos corregidos ni autorización de implementación.

## Dictamen

`Host.HIR` queda `AUDITADO` y `RECONSTRUIBLE` para diseñar la suite nueva. No se afirma ejecución, cobertura ni
ausencia de defectos del producto.
