# Tanda 02.04 — Capa canónica `robustness`

Fecha: 2026-10-09

## Problema

La Fase 1 asignó catorce casos `TA-AST-217..230` a la capa `robustness`. El manifiesto y el ejecutor C++ de la
Fase 2 conservaron ese contrato, pero el cargador estricto sólo aceptaba `unit` y `component` para el ejecutor
host. Esa discrepancia impedía validar el productor del gate de `Host.AST`.

## SCOPE LOCK y causa

- Entrada: una fila host con `layer=robustness`.
- Operación fallida: validación estricta de `_parse_case`.
- Invariante: toda capa publicada por el inventario canónico debe ser expresable sin reclasificar el caso.
- Símbolo responsable: `LAYERS` y el conjunto permitido para `nsc_component_tests`.
- Cambio mínimo: admitir `robustness` en esquema y cargador, y congelarlo con una prueba unitaria.
- Protegido: compilador, módulos AST, suite histórica, seis archivos preexistentes, `C:`, deploy y remotos.

No se cambió la semántica de selección, ejecución, aislamiento ni perfiles. `robustness` sigue siendo una capa
host ordinaria, ejecutada en proceso y sin optimizaciones de compilador.

## Validación prevista

La prueba `test_accepts_host_robustness_layer` demuestra que el cargador conserva la etiqueta. Después del commit
se ejecutarán las pruebas del controlador y el gate completo de `Host.AST`.

## Complejidad y recursión

El cambio añade una pertenencia a conjuntos constantes. Su costo permanece O(1), no asigna estructuras por caso
y no introduce recursión.
