# Tanda de Validacion 90: enteros exactos del reporte enfocado

Fecha: 10 de octubre de 2026
Estado: LISTA; VALIDACION FUNCIONAL EXTERNA PENDIENTE

## Causa confirmada

`parse_identity_envelope` comparaba `schema` y `run.selected` mediante igualdad
con `1`. En Python, `True == 1`; por tanto, un reporte con booleanos podia
acreditar tanto la version del esquema como la cardinalidad de seleccion.

## Correccion

- `schema` debe ser exactamente de tipo `int` y valer `1`;
- `run.selected` debe ser exactamente de tipo `int` y valer `1`;
- las demas dimensiones del reporte permanecen sin cambios.

## Evidencia ejecutada

- pruebas dirigidas del reporte enfocado: `6/6` PASS;
- suite Python completa del controlador: `226/226` PASS;
- booleanos en ambos campos son rechazados;
- no se configuro, compilo ni ejecuto NSC;
- no se ejecuto baseline ni se escribio en `C:`;
- los seis archivos protegidos conservaron sus hashes.

## Frontera de la afirmacion

La validacion demuestra tipos enteros exactos para esquema y cardinalidad del
reporte enfocado. No acredita una corrida funcional del producto. La meta
permanece activa hasta cero errores inesperados en la matriz acordada.
