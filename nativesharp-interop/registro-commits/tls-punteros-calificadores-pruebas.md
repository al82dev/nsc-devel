# Controles de calificadores de punteros TLS

El guion contrasta el IR de accesos `volatile` y exige rechazo al reasignar
`int* const` y al escribir mediante `const int*`, en estático y DLL. Los
casos positivos se ejecutan de O0 a O3 antes de los negativos. Pendiente la
construcción y validación agrupadas; no se acredita el conjunto por este guion.
Con NSC `d797816e3889d614` pasaron O0–O3 en estático y DLL y los controles
negativos. El IR contiene escritura `volatile i32` al destino y acceso
`volatile` al slot; la comprobación distingue ambas operaciones.
