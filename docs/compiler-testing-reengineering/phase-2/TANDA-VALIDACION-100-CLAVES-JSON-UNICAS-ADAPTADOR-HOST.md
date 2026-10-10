# Tanda de Validacion 100: claves JSON unicas en adaptador host

Fecha: 10 de octubre de 2026
Estado: LISTA; VALIDACION FUNCIONAL EXTERNA PENDIENTE

## Causa confirmada

`HostExecutor` analizaba la linea JSON de `nsc_component_tests` mediante la
politica last-wins. Una identidad, estado o evidencia podia declararse dos
veces y la segunda definicion sustituia silenciosamente a la primera antes de
validar el contrato terminal.

## Correccion

- el adaptador host canonico exige claves unicas en todos los objetos JSON;
- una clave repetida produce `INFRASTRUCTURE_ERROR` con la clave causal;
- el diagnostico previo de JSON sintacticamente invalido se conserva;
- productor, seleccion, ejecucion y bateria historica permanecen sin cambios.

## Evidencia ejecutada

- pruebas dirigidas de `HostExecutor`: `11/11` PASS;
- suite Python completa del controlador: `234/234` PASS;
- dos definiciones de `status` son rechazadas sin aplicar last-wins;
- no se configuro, compilo ni ejecuto NSC;
- no se ejecuto baseline ni se escribio en `C:`;
- los seis archivos protegidos conservaron sus hashes.

## Frontera de la afirmacion

La validacion demuestra unicidad de claves en la salida JSON consumida por el
adaptador host canonico. No acredita una corrida funcional del producto. La
meta permanece activa hasta cero errores inesperados en la matriz acordada.
