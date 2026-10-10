# Tanda de Validacion 73: razon de cancelacion tipada

Fecha: 10 de octubre de 2026
Estado: LISTA; VALIDACION FUNCIONAL EXTERNA PENDIENTE

## Causa confirmada

`CancellationToken.request` rechazaba valores falsos, pero aceptaba enteros y
booleanos verdaderos. Esa razon no textual podia alcanzar la evidencia de un
`CaseResult.CANCELLED` y romper el cierre causal de los casos pendientes.

## Correccion

- la razon debe ser una cadena;
- debe contener al menos un caracter no blanco;
- una entrada invalida falla antes de activar el token;
- la primera razon valida sigue siendo la unica autoridad.

## Evidencia ejecutada

- pruebas dirigidas de eventos y runner: `11/11` PASS;
- vacio, espacios, entero, booleano y `None`: rechazo sin activar el token;
- suite Python completa del controlador: `213/213` PASS;
- no se configuro, compilo ni ejecuto NSC;
- no se ejecuto baseline ni se escribio en `C:`;
- los seis archivos protegidos conservaron sus hashes.

## Frontera de la afirmacion

La validacion demuestra el tipo y atomicidad de la solicitud de cancelacion.
No acredita cancelacion de procesos reales. La meta permanece activa hasta
cero errores inesperados en la matriz acordada.
