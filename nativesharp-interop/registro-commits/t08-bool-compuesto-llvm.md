# T08-015: descenso LLVM de bit a bit compuesto sobre `_Bool` C

Para el arreglo C global `volatile _Bool` identificado por el clasificador
existente, el descenso de `&=`, `|=` y `^=` con bool carga el byte con
`volatile acquire`, convierte a i1, aplica la operación y almacena el resultado
como byte con `volatile release`. Cada operando vale cero o uno, por lo que
esas tres operaciones en i1 producen el mismo valor convertido a `_Bool` que
las promociones C de esos operandos. No se emite una actualización RMW.

El cambio de análisis es `612c8e472`. La cadena todavía requiere la prueba
positiva, construcción incremental y validación agrupada; no se afirma soporte
hasta que pasen. No cambia otros operandos ni el bool nativo.
