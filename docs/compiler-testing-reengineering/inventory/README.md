# Inventarios mecánicos

Esta carpeta conserva los universos canónicos y sus estados. Los archivos tabulares son generados de forma
determinista desde la revisión congelada y no sustituyen la lectura semántica archivo por archivo.

## Inventarios

- `production-files.csv`: código, contratos, build, dependencias y herramientas que forman o soportan NSC.
- `production-files.json`: la misma información en formato legible por máquina.
- `historical-tests-freeze.csv`: huellas de la batería histórica protegida; no es una migración ni un test plan.
- `components.csv`: agregación por rol y componente derivada del manifiesto de archivos.
- `symbols.csv`: inventario semántico de tipos, funciones, constantes y helpers relevantes.
- `contracts.csv`: contratos observables con invariantes, errores, dependencias y oráculos.
- `test-case-design.csv`: casos de prueba diseñados; no contiene ni genera pruebas ejecutables.
- `transversal-contracts.csv`: una fila por contrato con tratamiento explícito de los seis ejes de Tanda 11.
- `error-taxonomy.csv`: clasificación no exclusiva de errores y políticas de recuperación.
- `state-machines.csv`: estados, transiciones, invariantes, fallos y oráculos de doce ciclos estables.
- `state-machine-contract-map.csv`: relaciones auditadas entre los contratos y esas máquinas.

Los estados documentales válidos son los de la sección 18 de los lineamientos. El estado inicial es
`NO LEÍDO`; abrir un archivo no permite promoverlo directamente a `AUDITADO`.

Al cierre de Tanda 11, los 24 componentes host poseen inventario semántico, contratos, casos diseñados y segunda
auditoría independiente; los 870 contratos están además clasificados transversalmente. Ninguna fila `DESIGNED`
representa código de prueba creado o ejecutado.

<!-- BEGIN TANDA-12A-SELFHOST-INVENTORY -->
## Tanda 12A — self-host

- `selfhost-file-audit.csv`: cierre de los 29 archivos de `selfhost/nsc/**`.
- `symbols.csv`: 874 declaraciones físicas self-host añadidas con contrato primario.
- `contracts.csv`: 138 contratos self-host.
- `test-case-design.csv`: 1027 casos futuros self-host.
- Los conteos se recalculan con `evidence/TANDA-12A-SELFHOST-GENERATOR.ps1`.
<!-- END TANDA-12A-SELFHOST-INVENTORY -->

<!-- BEGIN TANDA-12B-REMAINING-SURFACE-INVENTORY -->
## Tanda 12B — superficies restantes

- `remaining-surface-file-audit.csv`: disposición de los 421 archivos.
- `remaining-surface-audit-summary.json`: 2233 declaraciones/artefactos, 301 contratos y 1806 casos diseñados.
- Los upstream y excluidos no generan pruebas ficticias; su frontera queda en los documentos de componente.
- Regeneración: `evidence/TANDA-12B-REMAINING-SURFACE-GENERATOR.ps1`.
<!-- END TANDA-12B-REMAINING-SURFACE-INVENTORY -->

<!-- BEGIN TANDA-12C-SUITE-ARCHITECTURE-INVENTORY -->
## Tanda 12C — arquitectura de la suite

- `suite-case-allocation.csv`: 10,193 casos asignados exactamente una vez a ejecutor, familia, perfil, modos,
  optimizaciones, aislamiento y autoridad.
- `implementation-waves.csv`: los 57 componentes ordenados en 11 olas o disposición externa.
- `suite-architecture-summary.json`: resumen reproducible de ejecutores, perfiles y capas.
- Regeneración: `evidence/TANDA-12C-SUITE-ARCHITECTURE-GENERATOR.ps1`.
<!-- END TANDA-12C-SUITE-ARCHITECTURE-INVENTORY -->
