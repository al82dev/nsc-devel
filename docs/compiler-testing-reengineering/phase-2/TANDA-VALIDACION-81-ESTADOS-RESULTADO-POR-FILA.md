# Tanda de Validacion 81: estados de resultado por fila

Fecha: 10 de octubre de 2026
Estado: LISTA; VALIDACION FUNCIONAL EXTERNA PENDIENTE

## Causa confirmada

`assert_component_results` comprobaba cobertura de IDs y el objeto agregado
`counts`, pero no leia `status` en cada fila. Un payload podia publicar todas
las filas como `FAIL`, declarar conteos `PASS` y `KNOWN_FAILURE` esperados y
superar el gate.

## Correccion

- cada fila exige un estado textual no vacio;
- los estados se cotejan en orden contra `expected_outcome` de la seleccion;
- los conteos se derivan tambien de las filas publicadas;
- `counts`, filas y resultados declarados deben coincidir entre si.

## Evidencia ejecutada

- pruebas dirigidas del soporte y gate de infraestructura: `12/12` PASS;
- suite Python completa del controlador: `221/221` PASS;
- el payload sintetico con filas `FAIL` y conteos exitosos es rechazado;
- no se configuro, compilo ni ejecuto NSC;
- no se ejecuto baseline ni se escribio en `C:`;
- los seis archivos protegidos conservaron sus hashes.

## Frontera de la afirmacion

La validacion demuestra consistencia interna exacta de resultados por fila y
conteos. No acredita una corrida funcional del producto. La meta permanece
activa hasta cero errores inesperados en la matriz acordada.
