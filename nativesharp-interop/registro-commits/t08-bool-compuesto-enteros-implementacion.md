# T08-015: `_Bool` C compuesto con `uint`, `long` y `ulong`

El análisis, HIR y LLVM amplían la ruta existente del arreglo C global
`volatile _Bool` en modo MS a `uint`, `long` y `ulong` para operaciones
aritméticas y bit a bit. LLVM promueve el bool al ancho del operando y usa su
signo para la operación, antes de normalizarlo a cero/no cero y almacenar el
byte. Los desplazamientos con esos operandos no se habilitan.

Está preparado sin construir ni validar. Faltan controles para valores altos,
división y residuo con signo y sin signo, y validación agrupada. El bool nativo
y NSLink no se modifican.
