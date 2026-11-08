# Calificador de record en typedef de función

El control de dos productores precompilados se amplió para comparar typedefs
de función variádica cuyo parámetro es `const CRecord*`. Con los encabezados
ocultos, ambas firmas iguales compilan y ejecutan en O0. Si solo una conserva
`const`, el consumidor rechaza la firma incompatible y no publica ejecutable.
El control queda incorporado a `typedef-funcion-record/validar.cmd`.

La prueba comprueba la declaración y resolución del typedef; no demuestra la
invocación mediante ese puntero ni otros calificadores. No fue necesario cambiar
el compilador. T08-002 y la tanda 8 siguen abiertos.
