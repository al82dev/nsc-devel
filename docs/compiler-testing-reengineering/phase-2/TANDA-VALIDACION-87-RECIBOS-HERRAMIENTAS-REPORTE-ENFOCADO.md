# Tanda de Validacion 87: recibos de herramientas del reporte enfocado

Fecha: 10 de octubre de 2026
Estado: LISTA; VALIDACION FUNCIONAL EXTERNA PENDIENTE

## Causa confirmada

`parse_identity_envelope` validaba corrida, caso, resultado, conteos y codigo
de salida, pero ignoraba por completo `run.tools`. Un reporte podia omitir o
sustituir el NSC seleccionado y aun acreditar el caso enfocado.

## Correccion

- Builder 3 calcula antes de la corrida el recibo esperado de cada herramienta
  seleccionada: nombre, ruta resuelta y SHA-256;
- el parser exige una lista de recibos con esquema cerrado;
- se rechazan campos vacios, duplicados, omisiones, sustituciones y campos
  adicionales;
- NSC es obligatorio; el comparador y el encabezado se incorporan solamente
  cuando fueron configurados para esa misma corrida.

## Evidencia ejecutada

- pruebas dirigidas del reporte enfocado: `6/6` PASS;
- suite Python completa del controlador: `225/225` PASS;
- recibo exacto aceptado; omision, hash distinto, duplicado y campo adicional
  rechazados;
- no se configuro, compilo ni ejecuto NSC;
- no se ejecuto baseline ni se escribio en `C:`;
- los seis archivos protegidos conservaron sus hashes.

## Frontera de la afirmacion

La validacion demuestra identidad exacta de las herramientas declaradas por el
reporte enfocado de Builder 3. No acredita una corrida funcional del producto.
La meta permanece activa hasta cero errores inesperados en la matriz acordada.
