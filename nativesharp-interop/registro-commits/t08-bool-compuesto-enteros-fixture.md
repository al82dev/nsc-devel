# T08-015: controles para `_Bool` C compuesto con enteros anchos

El fixture prepara operaciones aritméticas y bit a bit con `uint`, `long` y
`ulong`. Incluye división y residuo, valores negativos y valores con bits por
encima de 32 para detectar truncamiento antes de convertir el resultado a bool.
El verificador exige los accesos de byte, promociones i32/i64 y división y
residuo con signo y sin signo en IR. Los casos nativos se preservan.

Esta preparación aún no se ha construido ni ejecutado. Desplazamientos con
operandos anchos y demás orígenes están fuera de este control.
