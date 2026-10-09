# Segunda auditoría independiente — Host.Diagnostics

## Método

La segunda revisión recorrió los fuentes desde cero y contrastó cada tipo, función, overload, helper, lambda y
grupo de estado contra `symbols.csv`. Después recorrió los caminos de cada severidad, límite, append, clave de
merge, retorno del renderer y formato de salida.

## Resultado cuantitativo

| Control | Resultado obtenido |
|---|---:|
| archivos del componente | 8 |
| líneas congeladas | 764 |
| identificadores diagnósticos | 74 |
| símbolos consolidados | 43 |
| contratos consolidados | 12 |
| casos diseñados | 75 |
| archivos sin símbolos | 0 |
| símbolos sin contrato | 0 |
| contratos sin caso | 0 |
| IDs duplicados | 0 |
| referencias de línea fuera de archivo | 0 |
| diferencias de hash de producción | 0 |

## Contraste causal

Se comprobaron de forma separada:

- los cuatro caminos de conteo y el camino sin contador de `Note`;
- la transición exacta al límite y la supresión global posterior;
- las tres rutas de append, incluido el rango sintetizado;
- los siete campos de clave de merge y el empate no total;
- el encabezado con/sin código, fallback de fuente, tabs y rangos;
- el retorno temprano que también omite etiquetas y notas;
- el reset completo salvo la configuración de límite;
- ownership del modelo y referencias no propietarias del renderer.

## Hallazgos no corregidos

1. el merge no define orden para claves iguales con secundarios diferentes;
2. el retorno temprano del renderer omite evidencia secundaria;
3. un `Diagnostic` incoherente construido directamente puede producir una resta unsigned en longitud de marker;
4. la supresión después del límite afecta también warnings, lints y notes.

Son conducta/riesgo vigentes, no cambios autorizados. Los casos futuros deben caracterizarlos antes de decidir
si el contrato cambia.

## Dictamen

Los controles mecánicos confirmaron exactamente los valores de la tabla. `Host.Diagnostics` es `AUDITADO` y
`RECONSTRUIBLE` para la revisión congelada. Este dictamen no equivale a pruebas implementadas o ejecutadas.
