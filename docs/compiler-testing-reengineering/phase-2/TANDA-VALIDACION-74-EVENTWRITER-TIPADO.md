# Tanda de Validacion 74: EventWriter tipado

Fecha: 10 de octubre de 2026
Estado: LISTA; VALIDACION FUNCIONAL EXTERNA PENDIENTE

## Causa confirmada

`EventWriter.emit` confiaba en anotaciones y construia el payload mediante
`payload or {}`. Un `kind` textual fallaba tarde, un `case_id` numerico podia
serializarse y un contenedor falso como `[]` se convertia silenciosamente en un
objeto vacio.

## Correccion

- `kind` debe ser `EventKind`;
- `case_id` debe ser `None` o cadena no vacia;
- `payload` debe ser `None` u objeto;
- una entrada invalida no escribe ni consume numero de secuencia;
- se conserva escritura completa, flush y fsync por evento valido.

## Evidencia ejecutada

- pruebas dirigidas de eventos: `6/6` PASS;
- tres formas invalidas seguidas conservan la siguiente secuencia en cero;
- suite Python completa del controlador: `214/214` PASS;
- no se configuro, compilo ni ejecuto NSC;
- no se ejecuto baseline ni se escribio en `C:`;
- los seis archivos protegidos conservaron sus hashes.

## Frontera de la afirmacion

La validacion demuestra el esquema previo a serializacion de eventos. No
acredita ejecucion funcional de casos. La meta permanece activa hasta cero
errores inesperados en la matriz acordada.
