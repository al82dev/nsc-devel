# Tanda de Validacion 96: claves JSON unicas del reporte enfocado

Fecha: 10 de octubre de 2026
Estado: LISTA; VALIDACION FUNCIONAL EXTERNA PENDIENTE

## Causa confirmada

`parse_identity_envelope` analizaba `results.json` mediante la politica
last-wins de `json.loads`. Una clave repetida podia sustituir metadata, recibos,
conteos, estado o evidencia antes del cotejo estricto del reporte.

## Correccion

- todos los objetos del reporte se construyen con claves unicas;
- la regla alcanza objetos anidados y recibos de herramientas;
- una segunda definicion de `status` se rechaza antes de interpretar el
  resultado.

## Evidencia ejecutada

- pruebas dirigidas del reporte enfocado: `7/7` PASS;
- suite Python completa del controlador: `230/230` PASS;
- `status` duplicado es rechazado;
- no se configuro, compilo ni ejecuto NSC;
- no se ejecuto baseline ni se escribio en `C:`;
- los seis archivos protegidos conservaron sus hashes.

## Frontera de la afirmacion

La validacion demuestra unicidad de claves en el reporte enfocado consumido por
Builder 3. No acredita una corrida funcional del producto. La meta permanece
activa hasta cero errores inesperados en la matriz acordada.
