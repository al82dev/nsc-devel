# T08: validación inicial de calificadores flexibles

La construcción incremental con cambios hasta `53c6b1c78` terminó con código 0.
El primer control falló porque la sintaxis `int volatile*` no fue aceptada por
el parser; `688a7e1cc` sustituyó ese caso por un parámetro C `volatile int*`.
El validador requirió finales CRLF para que CMD encontrara su etiqueta.

Tras ese ajuste, `validar-decay.cmd` y `validar.cmd` terminaron con código 0.
En O0–O3 se leyó el arreglo `const` mediante `int const*`, se pasó el miembro
`volatile` a una función C calificada y se observaron sus lecturas/escrituras;
se rechazó descartar cada calificador y la conversión empaquetada subalineada.

No se verificó la semántica de un alias local volatile ni las rutas
precompiladas para calificadores. El caso no cierra el ítem ni la tanda 8.
