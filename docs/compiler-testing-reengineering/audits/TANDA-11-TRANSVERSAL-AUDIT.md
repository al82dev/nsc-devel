# Segunda auditoría independiente — Tanda 11

## Método

Se reimportaron los cinco inventarios canónicos, se contrastaron todos los IDs contra `contracts.csv`, se
validaron vocabularios, referencias documentales, cardinalidades, hashes y asignaciones de máquinas. La revisión
conceptual se hizo después de la generación inicial para no aceptar coincidencias textuales como semántica.

## Gates cuantitativos

| Gate | Resultado |
|---|---:|
| Contratos fuente / transversal / error | 870 / 870 / 870 |
| IDs de contrato duplicados en matrices 1:1 | 0 |
| Referencias a contratos inexistentes | 0 |
| Campos transversales vacíos | 0 |
| Máquinas / relaciones | 12 / 515 |
| Referencias a máquinas inexistentes | 0 |
| Contratos con máquina / stateless-value | 404 / 466 |
| Ejes sin `NOT_APPLICABLE` explícito | 0 |
| Contratos o casos nuevos | 0 / 0 |
| Filas históricas con schema reparado / pendientes | 22 / 0 |
| Casos consumidores revisados / afectados | 166 / 0 |

## Gates cualitativos

- Las seis dimensiones exigidas por el plan maestro están representadas.
- Las 22 filas antiguas desalineadas recuperaron `invariants`, `errors`, efectos, dependencias, oráculo, capa,
  estado, documento y sección en sus columnas correctas.
- Los 166 casos de esos contratos ya expresaban los oráculos propietarios y no requirieron modificación.
- Error, recuperación y estado permanecen separados; una función que devuelve error no se convierte por ello en
  máquina de estado.
- La pertenencia a máquinas se basa en contrato propietario, no en campos genéricos de un componente.
- Los falsos positivos por substrings fueron corregidos y el generador conserva los límites de palabra.
- Los 466 contratos sin ciclo propio siguen presentes y auditados como `STATELESS_OR_VALUE_CONTRACT`.
- Una relación múltiple no fusiona caché, job, pipeline, proceso o transacción en una máquina monolítica.
- No se duplicaron casos ni se alteraron los 7,360 diseños existentes.
- Ninguna afirmación se promovió a ejecución o cobertura.

## Hallazgos conservados

- Los contratos de componentes recientes contienen campos plantilla amplios. La matriz transversal conserva esos
  compromisos declarados, pero la pertenencia a máquinas usa evidencia más estrecha para evitar sobreclasificación.
- Ciertos labels de error son acumulativos y deliberadamente no exclusivos; sumar sus conteos sería incorrecto.
- La selección mínima de configuraciones y frecuencias se resolverá en Tanda 12, no se anticipa aquí.

## Dictamen

La Tanda 11 queda `AUDITADA`: 870/870 contratos tienen tratamiento transversal, taxonomía de error y estado
explícito, con cero omitidos, desconocidos o referencias rotas. La Fase 1 continúa con la arquitectura de la
suite nueva; no se afirma implementación ni ejecución.
