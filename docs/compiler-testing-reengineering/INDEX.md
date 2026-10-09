# Índice — Reingeniería de pruebas de NSC

## Orden de lectura

1. [SCOPE-LOCK.md](SCOPE-LOCK.md)
2. [PHASE-1-MASTER-PLAN.md](PHASE-1-MASTER-PLAN.md)
3. [TRACKING.md](TRACKING.md)
4. [inventory/README.md](inventory/README.md)
5. [architecture/COMPONENT-GRAPH.md](architecture/COMPONENT-GRAPH.md)
6. Documentos de componentes en `components/`
7. Planes de prueba en `test-plans/`
8. Informes de auditoría independiente en `audits/`

## Artefactos mecánicos

- `inventory/production-files.csv` y `.json`: manifiesto canónico de producción y soporte.
- `inventory/components.csv`: agregación de 57 componentes documentales.
- `inventory/historical-tests-freeze.csv`: batería histórica protegida por huellas.
- `inventory/document-coverage.csv`: trazabilidad entre archivo, símbolo, contrato y sección.
- `inventory/claims.csv`: catálogo de afirmaciones y procedencia.
- `CURSOR.json`: punto exacto de reanudación.

## Registros transversales

- [CONTRADICTIONS.md](CONTRADICTIONS.md)
- [BLOCKERS.md](BLOCKERS.md)
- [RECONSTRUCTIBILITY.md](RECONSTRUCTIBILITY.md)

## Leyenda de afirmaciones

Los documentos distinguen `HECHO COMPROBADO`, `INFERENCIA`, `DISEÑO PROPUESTO`, `IMPLEMENTACIÓN VIGENTE` y
`NO VERIFICADO`. Los planes futuros no se presentan como pruebas implementadas.
