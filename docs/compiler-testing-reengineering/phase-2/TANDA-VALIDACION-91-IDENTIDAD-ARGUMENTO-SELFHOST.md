# Tanda de Validacion 91: identidad del argumento self-host

Fecha: 10 de octubre de 2026
Estado: LISTA; VALIDACION FUNCIONAL EXTERNA PENDIENTE

## Causa confirmada

`parse_selfhost_inputs` exigia un `case_argument` no vacio, pero no demostraba
que correspondiera al `case_id` seleccionado. Una fila podia ejecutar otro
caso del mismo fixture y acreditarse solamente por coincidir con su oraculo de
salida.

## Correccion

- el parser recibe la identidad seleccionada;
- `case_argument` debe coincidir exactamente con ese `case_id`;
- `_run_selfhost` entrega la identidad de la variante actual;
- el inventario vivo confirma la relacion en los 192 casos self-host
  ejecutables.

## Evidencia ejecutada

- pruebas dirigidas de entradas self-host: `5/5` PASS;
- suite Python completa del controlador: `226/226` PASS;
- argumento ajeno sintetico rechazado;
- inventario vivo: `192/192` identidades coincidentes;
- no se configuro, compilo ni ejecuto NSC;
- no se ejecuto baseline ni se escribio en `C:`;
- los seis archivos protegidos conservaron sus hashes.

## Frontera de la afirmacion

La validacion demuestra que cada ruta self-host solicita el mismo caso que
pretende acreditar. No acredita una corrida funcional del producto. La meta
permanece activa hasta cero errores inesperados en la matriz acordada.
