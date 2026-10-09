# Estado de reconstructibilidad

Todos los componentes comienzan como `NO VERIFICADO` y sólo pueden llegar a `RECONSTRUIBLE` después de
responder las veinte preguntas aplicables, cerrar su matriz de contratos y superar una segunda auditoría
independiente.

El estado canónico por componente se conserva en `inventory/components.csv`. Este documento no sustituye esa
tabla; registra el criterio y enlazará los informes individuales conforme se produzcan.

## Componentes auditados

| Componente | Resultado | Símbolos | Contratos | Casos diseñados | Evidencia |
|---|---|---:|---:|---:|---|
| `Host.Basic` | `RECONSTRUIBLE` | 60 | 16 | 88 | `audits/TANDA-03-HOST-BASIC-AUDIT.md` |

`RECONSTRUIBLE` describe suficiencia documental para la revisión congelada. No significa que la suite nueva
esté implementada ni que se haya ejecutado validación.
