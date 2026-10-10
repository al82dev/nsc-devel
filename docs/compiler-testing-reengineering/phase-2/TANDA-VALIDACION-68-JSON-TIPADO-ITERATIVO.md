# Tanda de Validacion 68: JSON tipado e iterativo

Fecha: 10 de octubre de 2026
Estado: LISTA; VALIDACION FUNCIONAL EXTERNA PENDIENTE

## Causa confirmada

SelfHost.Diagnostics comparaba el JSON observado y esperado con la igualdad
normal de Python. Esa igualdad considera equivalentes `true` y `1`, asi como
`1` y `1.0`, por lo que una salida con tipo incorrecto podia recibir `PASS`.

## Correccion

- `json_values_equal_exact` compara tipo y valor en cada nodo;
- objetos requieren el mismo conjunto de claves;
- arreglos requieren la misma longitud y orden;
- el recorrido usa una lista de trabajo explicita y no usa recursion;
- Diagnostics aplica el comparador al oraculo JSON existente.

## Evidencia ejecutada

- pruebas dirigidas del oraculo self-host: `4/4` PASS;
- `{"version": 1}` frente a `{"version": true}`: rechazo demostrado;
- suite Python completa del controlador: `209/209` PASS;
- no se configuro, compilo ni ejecuto NSC;
- no se ejecuto baseline ni se escribio en `C:`;
- los seis archivos protegidos conservaron sus hashes.

## Frontera de la afirmacion

La validacion demuestra igualdad JSON exacta en el gate Diagnostics. No
acredita la ejecucion funcional del fixture. La meta permanece activa hasta
cero errores inesperados en la matriz acordada.
