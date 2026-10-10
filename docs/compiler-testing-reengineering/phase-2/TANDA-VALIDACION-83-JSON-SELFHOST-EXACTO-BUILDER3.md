# Tanda de Validacion 83: JSON self-host exacto en Builder 3

Fecha: 10 de octubre de 2026
Estado: LISTA; VALIDACION FUNCIONAL EXTERNA PENDIENTE

## Causa confirmada

La ruta monolitica `_run_selfhost` comparaba el JSON observado mediante la
igualdad permisiva de Python. Ese contrato podia acreditar `true` como si fuera
el entero `1`, o un entero como si fuera el flotante equivalente, aunque el
oraculo exige conservar tanto el tipo como el valor.

## Correccion

- se incorporo un comparador JSON iterativo y exacto por tipo;
- objetos y listas se recorren mediante una lista de trabajo, sin recursion;
- `_run_selfhost` usa el mismo contrato estricto para su oraculo JSON;
- la prueba dirigida cubre booleano frente a entero, entero frente a flotante
  y una estructura con 5000 niveles.

## Evidencia ejecutada

- pruebas dirigidas del oraculo self-host de Builder 3: `6/6` PASS;
- suite Python completa del controlador: `223/223` PASS;
- no se configuro, compilo ni ejecuto NSC;
- no se ejecuto baseline ni se escribio en `C:`;
- los seis archivos protegidos conservaron sus hashes.

## Frontera de la afirmacion

La validacion demuestra comparacion JSON exacta y no recursiva en la ruta
self-host monolitica de Builder 3. No acredita una corrida funcional del
producto. La meta permanece activa hasta cero errores inesperados en la matriz
acordada.
