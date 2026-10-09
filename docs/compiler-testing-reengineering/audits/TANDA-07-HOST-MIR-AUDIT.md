# Segunda auditoría independiente — Host.MIR

## Método

La segunda pasada comenzó en los 22 registros `PF-*`, no en la narrativa. Volvió a contar tipos, definiciones y
helpers, reconstruyó el flujo HIR → Builder → Module/Function/Block/Instruction/Value → Verifier/Dumper y
contrastó cada símbolo con contrato, documento y matriz de casos.

## Resultado cuantitativo

| Control | Valor |
|---|---:|
| archivos | 22 |
| líneas | 3,054 |
| bytes | 105,640 |
| tipos | 15 |
| definiciones calificadas | 123 |
| helpers de namespace | 14 |
| símbolos | 152 |
| contratos | 38 |
| casos diseñados | 304 |
| casos PR | 266 |
| casos NIGHTLY | 38 |
| omitidos o desconocidos | 0 |

## Gates

- cada archivo tiene huella, tamaño, líneas, estado, evidencia y sección;
- cada símbolo tiene archivo, línea, responsabilidad y contrato;
- cada contrato tiene productores, entradas, invariantes, errores, dependencias y oracle;
- cada contrato posee ocho matrices y cada caso referencia un contrato existente;
- CSV y JSON conservan el mismo inventario de 974 archivos;
- no se tocó código del compilador, tests, builders, C: o remotos;
- las huellas de los seis cambios preexistentes permanecen iguales.

## Hallazgos conservados

1. `MirBuilder` baja expresiones recursivamente y no contiene una worklist para profundidad adversarial.
2. `MirBuilder::build` no limpia el error de una instancia reutilizada.
3. `MirModule::findFunction` compara símbolos mediante búsqueda lineal.
4. `MirVerifier` comprueba membresía, tipos y definición única, pero no dominancia ni use-before-def.
5. La validez predecessor de las entradas PHI pertenece a componentes posteriores.
6. Los setters del modelo permiten estados incompletos hasta el verifier.

Son observaciones de la revisión congelada, no defectos corregidos ni autorización de implementación.

## Dictamen

`Host.MIR` queda `AUDITADO` y `RECONSTRUIBLE` para diseñar la suite nueva. No se afirma ejecución, cobertura ni
ausencia de defectos del producto.
