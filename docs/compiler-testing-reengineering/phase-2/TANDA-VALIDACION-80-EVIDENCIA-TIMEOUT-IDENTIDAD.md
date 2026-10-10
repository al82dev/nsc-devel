# Tanda de Validacion 80: evidencia de timeout de identidad

Fecha: 10 de octubre de 2026
Estado: LISTA; VALIDACION FUNCIONAL EXTERNA PENDIENTE

## Causa confirmada

`CompilerIdentityExecutor._invoke` escribia los logs solamente despues de que
`subprocess.run` terminara. Un `TimeoutExpired` saltaba esa publicacion y
perdia `stdout` y `stderr` parciales, aunque el recibo del comando si existiera.

## Correccion

- `_invoke` captura el timeout exclusivamente para publicar sus dos logs;
- la salida parcial en bytes se decodifica como UTF-8 con reemplazo estable;
- el mismo `TimeoutExpired` vuelve a propagarse al clasificador existente;
- el resultado terminal permanece `INFRASTRUCTURE_ERROR` por timeout.

## Evidencia ejecutada

- pruebas dirigidas del ejecutor de identidad: `6/6` PASS;
- suite Python completa del controlador: `220/220` PASS;
- la prueba sintetica conserva `partial-identity` y `partial-error`;
- no se configuro, compilo ni ejecuto NSC;
- no se ejecuto baseline ni se escribio en `C:`;
- los seis archivos protegidos conservaron sus hashes.

## Frontera de la afirmacion

La validacion demuestra la preservacion de evidencia parcial ante timeout del
proceso de identidad sintetico. No acredita timeouts de un NSC real ni una
corrida funcional. La meta permanece activa hasta cero errores inesperados en
la matriz acordada.
