# Tanda de Validación 18 — Cierre central de Host.Language

## Alcance causal

Veintiún gates de Host.Language repetían después de `run_suite.py` los rangos
de casos y los totales de estados de su contrato. Esos datos dependen de la
revisión y duplicaban la autoridad del manifiesto y del controlador central.

## Corrección

Se retiraron exclusivamente esas comprobaciones duplicadas y la lectura JSON
que sólo existía para sostenerlas. Cada gate conserva:

- el contrato y perfil originales;
- build mode, herramienta host y workspace temporal;
- el mismo controlador y el mismo rechazo de retorno no cero;
- los oráculos y `expected_outcome` de todos sus casos.

El selector exige que el contrato produzca una selección válida y no vacía; el
reporte exige cobertura exacta y el retorno del controlador rechaza cualquier
estado distinto de `PASS` o `KNOWN_FAILURE` declarado.

## Evidencia dirigida

- `21/21` gates: validación sintáctica PASS.
- copias locales residuales de IDs, rangos y conteos: `0`.
- selector, runner, ledger y reporte: `20/20` pruebas Python PASS.

## Límites preservados

- sin ejecución de producto ni gates funcionales;
- sin configuración, compilación, baseline, `C:`, despliegue ni `push`;
- ningún contrato, perfil, caso, oráculo o resultado esperado modificado;
- batería histórica y seis archivos protegidos intactos.
