# Tanda de Validacion 65: eventos sin coercion

Fecha: 10 de octubre de 2026
Estado: LISTA; VALIDACION FUNCIONAL EXTERNA PENDIENTE

## Causa confirmada

El gate sintetico de infraestructura comparaba secuencias y cardinalidades con
enteros sin exigir su tipo exacto y convertia los `case_id` mediante
`str(...)`. Por la igualdad numerica de Python, `false`/`true` podian acreditar
las secuencias `0`/`1` y `1.0` podia acreditar un conteo de uno.

## Correccion

- cada evento debe ser un objeto;
- cada secuencia debe ser un `int` exacto y conservar continuidad;
- cada identidad de evento de caso debe ser una cadena exacta no vacia;
- los payloads terminales deben ser objetos;
- `selected` y `completed` deben ser enteros exactos;
- se conserva la exigencia estricta de `cancelled is False`.

## Evidencia ejecutada

- pruebas dirigidas del gate de infraestructura: `4/4` PASS;
- suite Python completa del controlador: `204/204` PASS;
- no se configuro, compilo ni ejecuto NSC;
- no se ejecuto baseline ni se escribio en `C:`;
- los seis archivos protegidos conservaron sus hashes.

## Frontera de la afirmacion

La validacion cubre el protocolo sintetico de eventos del controlador. No
acredita ejecucion funcional de NSC ni de los casos. La meta sigue activa hasta
cero errores inesperados en la matriz acordada.
