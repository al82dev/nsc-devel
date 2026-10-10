# Tanda de Validación 17 — Cierre central de gates foundation

## Alcance causal

Veintisiete gates dirigidos de Host.Basic, Host.Diagnostics y Host.Source
ejecutaban un contrato mediante `run_suite.py` y después repetían una lista o
rango fijo de casos y un total fijo de resultados. Esa segunda comprobación no
era un oráculo de producto: duplicaba la selección y el cierre ya realizados
por la infraestructura central.

## Corrección

Cada gate conserva sin cambios:

- contrato y perfil seleccionados;
- build mode y herramientas requeridas;
- invocation del controlador y aislamiento temporal;
- rechazo de cualquier código de retorno distinto de cero.

Se retiraron únicamente la lectura redundante del reporte y las copias locales
de IDs y totales. `select_cases` exige una selección válida y no vacía,
`ReportWriter` exige exactamente un resultado por caso seleccionado y
`run_suite.run` sólo retorna cero cuando todos los resultados son `PASS` o
`KNOWN_FAILURE` declarado.

## Evidencia dirigida

- `27/27` gates: validación sintáctica PASS.
- copias locales residuales de IDs, rangos y conteos en esos gates: `0`.
- selector, runner, ledger y reporte: `20/20` pruebas Python PASS.
- gate especial de ownership de Host.Source: no modificado en esta tanda.

## Límites preservados

- sin ejecución de producto ni gates funcionales;
- sin configuración, compilación, baseline, `C:`, despliegue ni `push`;
- ningún contrato, perfil, caso, oráculo o resultado esperado modificado;
- batería histórica y seis archivos protegidos intactos.
