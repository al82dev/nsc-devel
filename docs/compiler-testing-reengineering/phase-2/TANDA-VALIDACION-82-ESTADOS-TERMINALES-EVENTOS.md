# Tanda de Validacion 82: estados terminales de eventos

Fecha: 10 de octubre de 2026
Estado: LISTA; VALIDACION FUNCIONAL EXTERNA PENDIENTE

## Causa confirmada

`validate_events` verificaba secuencia, cardinalidad y cobertura de IDs, pero no
el `status` publicado por cada `CASE_FINISHED`. Un stream podia cubrir todos los
casos y declarar estados distintos de sus outcomes sin ser rechazado.

## Correccion

- cada `CASE_FINISHED` exige payload objeto;
- su estado debe ser una cadena no vacia;
- el estado se coteja contra `expected_outcome` del caso identificado;
- la validacion precede al cierre de los eventos terminales de la corrida.

## Evidencia ejecutada

- pruebas dirigidas del gate de infraestructura: `5/5` PASS;
- suite Python completa del controlador: `222/222` PASS;
- el evento sintetico `FAIL` para un caso declarado `PASS` es rechazado;
- no se configuro, compilo ni ejecuto NSC;
- no se ejecuto baseline ni se escribio en `C:`;
- los seis archivos protegidos conservaron sus hashes.

## Frontera de la afirmacion

La validacion demuestra coherencia terminal por caso en el stream progresivo.
No acredita una corrida funcional del producto. La meta permanece activa hasta
cero errores inesperados en la matriz acordada.
