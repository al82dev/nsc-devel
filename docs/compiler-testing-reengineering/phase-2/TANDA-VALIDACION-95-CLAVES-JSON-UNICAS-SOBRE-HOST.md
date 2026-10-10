# Tanda de Validacion 95: claves JSON unicas del sobre host

Fecha: 10 de octubre de 2026
Estado: LISTA; VALIDACION FUNCIONAL EXTERNA PENDIENTE

## Causa confirmada

`parse_host_envelope` analizaba la linea terminal con la politica last-wins de
`json.loads`. Una clave repetida podia sustituir identidad, estado, duracion o
evidencia antes de que se aplicaran las invariantes del sobre.

## Correccion

- el sobre host usa el constructor local de objetos JSON con claves unicas;
- la regla alcanza cualquier objeto anidado futuro;
- una segunda definicion de `status` es rechazada antes de interpretar el
  resultado.

## Evidencia ejecutada

- pruebas dirigidas del sobre host: `7/7` PASS;
- suite Python completa del controlador: `229/229` PASS;
- `status` duplicado es rechazado;
- no se configuro, compilo ni ejecuto NSC;
- no se ejecuto baseline ni se escribio en `C:`;
- los seis archivos protegidos conservaron sus hashes.

## Frontera de la afirmacion

La validacion demuestra unicidad de claves en el sobre host consumido por
Builder 3. No acredita una corrida funcional del producto. La meta permanece
activa hasta cero errores inesperados en la matriz acordada.
