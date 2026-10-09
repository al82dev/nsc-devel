# Segunda auditoría independiente — Host.Frontend

## Método

La segunda pasada partió del manifiesto físico de 121 archivos. Reextrajo tipos, definiciones e inline bodies;
después reconstruyó productores y consumidores por modelo, verificó propiedad contractual y contrastó cada
contrato contra su matriz de casos. Los conteos se derivaron de archivos, no del documento narrativo.

## Resultado cuantitativo

| Control | Valor |
|---|---:|
| archivos | 121 |
| líneas | 74,688 |
| bytes | 3,623,063 |
| tipos definidos en headers | 271 |
| definiciones físicas de source | 1,101 |
| operaciones inline con conducta | 20 |
| símbolos | 1,392 |
| contratos | 63 |
| casos diseñados | 504 |
| casos de frecuencia PR | 441 |
| casos de frecuencia NIGHTLY | 63 |
| archivos/símbolos/contratos omitidos | 0 |
| IDs duplicados o referencias inválidas | 0 |

El conteo de casos se toma mecánicamente de `inventory/test-case-design.csv`; no se fijó como cuota ni se
inventó en este informe.

## Gates reproducidos

- 121/121 archivos tienen huella, tamaño, líneas y estado documental.
- 1,392/1,392 símbolos tienen archivo, línea, responsabilidad, contrato, documento y sección.
- 63/63 contratos tienen productores y casos diseñados.
- 504/504 casos tienen identificador único; cada contrato posee ocho matrices parametrizadas.
- cada caso referencia un contrato existente y declara entrada, esperado, capa y frecuencia;
- ningún símbolo de otro componente fue reclasificado como Frontend;
- CSV y JSON de producción continúan describiendo los mismos 974 archivos;
- sólo `docs/compiler-testing-reengineering/**` cambió dentro del trabajo autorizado;
- los seis cambios preexistentes conservaron sus huellas.

## Hallazgos conservados

1. Cuatro recorridos recursivos concretos permanecen en method bodies y existen otros en familias ya leídas.
2. El evaluador HIR de `static_assert` no representa realmente enteros de 128 bits.
3. La verificación `out` no demuestra asignación en toda ruta.
4. El plegado local para divisor cero no cubre toda expresión const derivada.
5. Dumpers de proyecto, cuerpos, HIR y MIR son vistas parciales.
6. La promoción a stack es conservadora, post-HIR y limitada a un MiB.
7. Textos históricos permanecen como superficie observable de la revisión congelada.

## Dictamen

`Host.Frontend` es `AUDITADO` y `RECONSTRUIBLE` para la revisión congelada. Esto expresa suficiencia documental
para diseñar la suite nueva; no afirma implementación, ejecución, coverage ni ausencia de defectos del producto.
