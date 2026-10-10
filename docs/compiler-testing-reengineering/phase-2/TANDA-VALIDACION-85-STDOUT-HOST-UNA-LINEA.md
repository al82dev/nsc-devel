# Tanda de Validacion 85: stdout host de una sola linea

Fecha: 10 de octubre de 2026
Estado: LISTA; VALIDACION FUNCIONAL EXTERNA PENDIENTE

## Causa confirmada

`parse_host_envelope` seleccionaba solamente las lineas cuyo contenido
comenzaba con `{`. Una salida con diagnosticos inesperados y un objeto JSON
valido podia reducirse artificialmente a una sola linea y acreditarse. El
productor real y el adaptador canonico exigen exactamente una linea no vacia.

## Correccion

- se cuentan todas las lineas no vacias de stdout;
- una linea adicional, aunque no parezca JSON, invalida el protocolo;
- la unica linea se analiza despues como objeto JSON.

## Evidencia ejecutada

- pruebas dirigidas del protocolo host: `5/5` PASS;
- suite Python completa del controlador: `223/223` PASS;
- ruido textual seguido de un objeto valido es rechazado por cardinalidad;
- el arreglo JSON de una sola linea es rechazado por tipo;
- no se configuro, compilo ni ejecuto NSC;
- no se ejecuto baseline ni se escribio en `C:`;
- los seis archivos protegidos conservaron sus hashes.

## Frontera de la afirmacion

La validacion demuestra cardinalidad exacta del stdout host en Builder 3. No
acredita una corrida funcional del producto. La meta permanece activa hasta
cero errores inesperados en la matriz acordada.
