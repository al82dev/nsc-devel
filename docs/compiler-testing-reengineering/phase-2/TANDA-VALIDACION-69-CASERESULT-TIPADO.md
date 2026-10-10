# Tanda de Validacion 69: CaseResult tipado

Fecha: 10 de octubre de 2026
Estado: LISTA; VALIDACION FUNCIONAL EXTERNA PENDIENTE

## Causa confirmada

`CaseResult` comprobaba algunas relaciones causales, pero confiaba en sus
anotaciones para los tipos. Era posible construir un resultado con
`status="PASS"`, `duration_seconds=true`, listas en lugar de tuplas o evidencia
no textual y registrarlo en el ledger.

## Correccion

- `case_id` debe ser una cadena no vacia;
- `status` debe ser una instancia de `CaseStatus`;
- la duracion debe ser `int` o `float` finito, no booleano y no negativo;
- `failures` y `blocked_by` deben ser tuplas de cadenas no vacias;
- los productores bloqueadores no pueden repetirse;
- se conservan las invariantes causales existentes de cada estado.

## Evidencia ejecutada

- pruebas dirigidas de resultados y ledger: `7/7` PASS;
- suite Python completa del controlador: `210/210` PASS;
- no se configuro, compilo ni ejecuto NSC;
- no se ejecuto baseline ni se escribio en `C:`;
- los seis archivos protegidos conservaron sus hashes.

## Frontera de la afirmacion

La validacion demuestra la forma exacta de los objetos `CaseResult`. No acredita
la ejecucion funcional de la matriz. La meta permanece activa hasta cero
errores inesperados en el conjunto acordado.
