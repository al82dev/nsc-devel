# Segunda auditoría independiente — Host.Source

## Método independiente

La segunda pasada partió de los cinco archivos fuente y no del documento narrativo. Para cada declaración y
definición se comprobó: presencia en `symbols.csv`, pertenencia a un contrato, caso futuro capaz de observar el
resultado y referencia de línea válida. Después se recorrieron por separado los caminos de éxito/fallo y las
fronteras de cada algoritmo.

## Resultado cuantitativo

| Control | Resultado obtenido |
|---|---:|
| archivos del componente | 5 |
| líneas congeladas | 330 |
| símbolos consolidados | 22 |
| contratos consolidados | 8 |
| casos diseñados | 58 |
| archivos sin símbolos | 0 |
| símbolos sin contrato | 0 |
| contratos sin caso | 0 |
| IDs duplicados | 0 |
| referencias de línea fuera de archivo | 0 |
| diferencias de hash de producción | 0 |

## Fronteras contrastadas

- vacío, EOF y offset fuera de rango;
- LF, CR, CRLF, terminador final y secuencias mixtas;
- columnas byte con contenido multibyte;
- tamaño 64 MiB, crecimiento y truncamiento durante lectura;
- ID cero, ID posterior al vector, rangos vacíos e inválidos;
- ownership de path/buffer y lifetime no propietario de punteros/vistas;
- texto de error preexistente en una operación exitosa;
- complejidad lineal de escaneo/lectura y logarítmica de posición.

## Hallazgos documentales

La revisión obliga a registrar, sin corregirlos, tres límites que una lectura superficial puede ocultar:

1. `addBuffer` no aplica el límite de 64 MiB ni valida overflow del ID;
2. `readFileBuffer` no limpia `errorMessage` cuando tiene éxito;
3. punteros, referencias y vistas no deben conservarse a través de una mutación estructural del manager.

No son autorización de implementación; quedan como contratos/riesgos explícitos para diseño y futura decisión.

## Dictamen

Los controles mecánicos arrojaron exactamente los valores indicados. `Host.Source` es `AUDITADO` y
`RECONSTRUIBLE` para la revisión congelada. El dictamen sólo cubre diseño documental; no afirma pruebas
implementadas ni ejecutadas.
