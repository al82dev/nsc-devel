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

## Componentes consolidados

- [Host.Basic](components/HOST-BASIC.md)
  - [Test plan](test-plans/HOST-BASIC-TEST-PLAN.md)
  - [Auditoría independiente](audits/TANDA-03-HOST-BASIC-AUDIT.md)
- [Host.Source](components/HOST-SOURCE.md)
  - [Test plan](test-plans/HOST-SOURCE-TEST-PLAN.md)
  - [Auditoría independiente](audits/TANDA-03-HOST-SOURCE-AUDIT.md)
- [Host.Diagnostics](components/HOST-DIAGNOSTICS.md)
  - [Test plan](test-plans/HOST-DIAGNOSTICS-TEST-PLAN.md)
  - [Auditoría independiente](audits/TANDA-03-HOST-DIAGNOSTICS-AUDIT.md)

## Artefactos mecánicos

- `inventory/production-files.csv` y `.json`: manifiesto canónico de producción y soporte.
- `inventory/components.csv`: agregación de 57 componentes documentales.
- `inventory/historical-tests-freeze.csv`: batería histórica protegida por huellas.
- `inventory/document-coverage.csv`: trazabilidad entre archivo, símbolo, contrato y sección.
- `inventory/symbols.csv`: símbolos, responsabilidades, contratos y capa futura de prueba.
- `inventory/contracts.csv`: contratos estables derivados de la implementación vigente.
- `inventory/test-case-design.csv`: casos lógicos diseñados, sin implementación ejecutable.
- `inventory/claims.csv`: catálogo de afirmaciones y procedencia.
- `CURSOR.json`: punto exacto de reanudación.

## Registros transversales

- [CONTRADICTIONS.md](CONTRADICTIONS.md)
- [BLOCKERS.md](BLOCKERS.md)
- [RECONSTRUCTIBILITY.md](RECONSTRUCTIBILITY.md)

## Leyenda de afirmaciones

Los documentos distinguen `HECHO COMPROBADO`, `INFERENCIA`, `DISEÑO PROPUESTO`, `IMPLEMENTACIÓN VIGENTE` y
`NO VERIFICADO`. Los planes futuros no se presentan como pruebas implementadas.
