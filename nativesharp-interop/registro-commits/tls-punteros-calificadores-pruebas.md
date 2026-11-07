# Controles de calificadores de punteros TLS

El guion contrasta el IR de accesos `volatile` y exige rechazo al reasignar
`int* const` y al escribir mediante `const int*`, en estático y DLL. Los
casos positivos se ejecutan de O0 a O3 antes de los negativos. Pendiente la
construcción y validación agrupadas; no se acredita el conjunto por este guion.
