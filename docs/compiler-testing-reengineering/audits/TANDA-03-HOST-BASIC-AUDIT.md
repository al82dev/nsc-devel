# Auditoría independiente — Host.Basic

## Resultado cuantitativo

| Gate | Resultado |
|---|---:|
| Archivos esperados/revisados | 12 / 12 |
| Líneas fuente | 939 |
| Símbolos tratados | 60 |
| Contratos consolidados | 16 |
| Casos lógicos diseñados | 88 |
| Hashes distintos al manifiesto | 0 |
| Archivos sin símbolo trazado | 0 |
| IDs duplicados | 0 |
| Referencias a contratos desconocidos | 0 |
| Líneas de evidencia fuera de rango | 0 |
| Huecos en IDs de casos | 0 |

## Segunda lectura

La segunda pasada no se limitó a comprobar conteos. Volvió a recorrer las doce fuentes contra el inventario y
detectó cuatro omisiones internas de la primera extracción: las dos operaciones de copia eliminadas, el estado
local por entrada y la lambda que limpia el preflight. Las cuatro quedaron documentadas antes de cerrar el gate.

No se encontraron otros tipos o callables pública o internamente relevantes sin fila. Los arrays de constantes,
variables automáticas y campos se documentan dentro del símbolo propietario; no se convierten artificialmente en
contratos independientes.

## Reconstructibilidad

Las veinte preguntas aplicables a la revisión congelada están respondidas. El componente conserva descripción de:

- valores, formatos y números públicos;
- algoritmos SHA-256 y transaccional;
- variantes Windows/no Windows;
- estados, rollback, errores y efectos;
- ownership, lifetime y consumidores;
- orden de reimplementación y test plan.

Resultado: `RECONSTRUIBLE` para la revisión congelada. No significa que los 88 casos estén implementados o que
se haya validado ejecución.
