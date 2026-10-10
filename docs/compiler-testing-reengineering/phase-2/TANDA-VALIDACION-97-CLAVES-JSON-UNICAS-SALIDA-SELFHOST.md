# Tanda de Validacion 97: claves JSON unicas en salida self-host

Fecha: 10 de octubre de 2026
Estado: LISTA; VALIDACION FUNCIONAL EXTERNA PENDIENTE

## Causa confirmada

La salida JSON de fixtures self-host se convertia mediante `json.loads` antes
de la comparacion exacta. Una clave repetida quedaba reducida a su ultima
definicion y el comparador ya no podia detectar la ambiguedad original.

## Correccion

- la salida del fixture pasa por un parser local con claves unicas;
- la regla alcanza objetos anidados;
- solamente despues se compara estructura, tipo y valor contra el oraculo.

## Evidencia ejecutada

- pruebas dirigidas del oraculo self-host: `7/7` PASS;
- suite Python completa del controlador: `231/231` PASS;
- salida valida aceptada y `status` duplicado rechazado;
- no se configuro, compilo ni ejecuto NSC;
- no se ejecuto baseline ni se escribio en `C:`;
- los seis archivos protegidos conservaron sus hashes.

## Frontera de la afirmacion

La validacion demuestra unicidad de claves en la salida JSON self-host antes
del oraculo. No acredita una corrida funcional del producto. La meta permanece
activa hasta cero errores inesperados en la matriz acordada.
