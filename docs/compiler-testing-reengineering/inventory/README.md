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

Los estados documentales válidos son los de la sección 18 de los lineamientos. El estado inicial es
`NO LEÍDO`; abrir un archivo no permite promoverlo directamente a `AUDITADO`.

Al cierre de Tanda 03, `Host.Basic`, `Host.Source` y `Host.Diagnostics` tienen inventario semántico, contratos,
casos diseñados y segunda auditoría independiente. Ninguna fila `DESIGNED` representa código de prueba creado
o ejecutado.
