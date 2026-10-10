# Tanda de Validación 19 — Cierre central de Host.Target

## Alcance causal

Treinta gates de Host.Target repetían después de `run_suite.py` rangos de casos
y totales de resultados propios de una revisión. Esas comprobaciones no eran
oráculos de Target; duplicaban selección, cobertura y estados ya garantizados
por la infraestructura compartida.

## Corrección

Se retiraron exclusivamente las copias locales de IDs y conteos, junto con la
lectura JSON que sólo las sostenía. Cada gate conserva:

- contrato, perfil, build mode y herramienta host;
- workspace aislado e invocación del mismo controlador;
- rechazo del código de retorno no cero;
- todos los oráculos de identidad, ABI, layout, alineación, bitfields, CPU,
  LLVM y registros contenidos en sus casos.

## Evidencia dirigida

- `30/30` gates: validación sintáctica PASS.
- copias locales residuales de IDs, rangos y conteos: `0`.
- selector, runner, ledger y reporte: `20/20` pruebas Python PASS.

## Límites preservados

- sin ejecución de producto ni gates funcionales;
- sin configuración, compilación, baseline, `C:`, despliegue ni `push`;
- ningún contrato, perfil, caso, oráculo o resultado esperado modificado;
- batería histórica y seis archivos protegidos intactos.
