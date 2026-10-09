# Segunda auditoría independiente — Host.Driver

## Método

La segunda pasada partió de los 51 registros `PF-*`. Recontó archivos, líneas, bytes, tipos y funciones; reagrupó
CLI, sesión, scheduler, productos, metadata, paquetes, instalación y migración; después contrastó documentos,
contratos, casos, formatos, diagnósticos y transacciones.

## Resultado cuantitativo

| Control | Valor |
|---|---:|
| archivos | 51 |
| líneas | 15,323 |
| bytes | 675,413 |
| tipos definidos | 61 |
| funciones definidas | 326 |
| símbolos | 387 |
| contratos | 77 |
| casos diseñados | 616 |
| casos PR | 539 |
| casos NIGHTLY | 77 |
| omitidos o desconocidos | 0 |

## Gates

- 51 archivos con huella, estado, evidencia y sección;
- 387 símbolos físicos con archivo, línea y contrato;
- 77 contratos con fuente y exactamente ocho matrices;
- 616 casos con IDs únicos y referencias válidas;
- todo símbolo asignado a un contrato y todo archivo representado en la matriz;
- hashes fuente iguales al manifiesto congelado;
- cero cambios en producto, pruebas, builders, C: o remotos;
- seis archivos preexistentes conservados byte por byte.

## Hallazgos conservados

1. El driver posee dispatch, productos, metadata, paquetes, instalación y release policy en una sola frontera física.
2. Tests futuros se atan a contratos observables, no a esa distribución monolítica.
3. Scheduler, worker pool y scopes definen orden, restauración y causa primaria deterministas.
4. Todos los productos complejos usan staging/transacción antes de publicación.
5. NSMETA se materializa y vuelve a parsearse antes de aceptar una clausura pública.
6. Restore, Content y lifecycle confinan paths y tienen rollback explícito.
7. Varias clausuras de metadata y el JSON local conservan recursión.
8. Ayuda, instalación y manifests de release conservan conceptos versionados históricos.

Son observaciones estáticas, no defectos corregidos.

## Dictamen

`Host.Driver` queda `AUDITADO` y `RECONSTRUIBLE`. No se afirma ejecución, cobertura ni ausencia de defectos de
producto.
