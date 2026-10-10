# Tanda de Validacion 79: evidencia de timeout host

Fecha: 10 de octubre de 2026
Estado: LISTA; VALIDACION FUNCIONAL EXTERNA PENDIENTE

## Causa confirmada

`subprocess.TimeoutExpired` puede conservar `stdout` y `stderr` parciales como
bytes aun cuando el proceso se lanzo con `text=True`. `HostExecutor` enviaba
esos bytes directamente a `Path.write_text`, lo que podia reemplazar el timeout
causal por un `TypeError` y perder sus logs parciales.

## Correccion

- `None` se normaliza como texto vacio;
- los bytes se decodifican como UTF-8 con reemplazo estable;
- las cadenas existentes se conservan sin modificacion;
- el resultado terminal sigue siendo `INFRASTRUCTURE_ERROR` por timeout.

## Evidencia ejecutada

- pruebas dirigidas del adaptador host: `10/10` PASS;
- suite Python completa del controlador: `219/219` PASS;
- la prueba sintetica conserva `partial-out` y `partial-err` en sus logs;
- no se configuro, compilo ni ejecuto NSC;
- no se ejecuto baseline ni se escribio en `C:`;
- los seis archivos protegidos conservaron sus hashes.

## Frontera de la afirmacion

La validacion demuestra la preservacion de evidencia parcial ante timeout del
host sintetico. No acredita timeouts del binario real ni una corrida funcional.
La meta permanece activa hasta cero errores inesperados en la matriz acordada.
