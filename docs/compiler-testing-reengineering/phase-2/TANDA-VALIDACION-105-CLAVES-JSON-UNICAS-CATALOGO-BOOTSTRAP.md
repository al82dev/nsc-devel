# Tanda de Validacion 105: claves JSON unicas en catalogo bootstrap

Fecha: 10 de octubre de 2026
Estado: LISTA; VALIDACION FUNCIONAL EXTERNA PENDIENTE

## Causa confirmada

`SelfHost.Bootstrap.Infrastructure` analizaba cada fila de su catalogo con
last-wins antes de validar linaje, cobertura y existencia de productores. Una
segunda identidad, ruta o lista de casos podia sustituir silenciosamente el
contrato declarado.

## Correccion

- cada fila se analiza mediante `parse_catalog_row` con claves unicas;
- la regla alcanza objetos anidados;
- archivo, numero de linea y clave duplicada permanecen en el diagnostico;
- la construccion del objeto es iterativa y no introduce recursion;
- catalogo vivo, inventario y validaciones de linaje permanecen sin cambios.

## Evidencia ejecutada

- prueba dirigida del soporte bootstrap: `1/1` PASS;
- suite Python completa del controlador: `239/239` PASS;
- una segunda definicion de `case_ids` se rechaza en la linea indicada;
- no se configuro, compilo ni ejecuto NSC;
- no se ejecuto baseline ni se escribio en `C:`;
- los seis archivos protegidos conservaron sus hashes.

## Frontera de la afirmacion

La validacion demuestra unicidad de claves al leer el catalogo bootstrap
infrastructure. No acredita una corrida funcional del gate ni del producto. La
meta permanece activa hasta cero errores inesperados en la matriz acordada.
