# T08-015: promociones enteras restantes de `_Bool` C compuesto

El análisis y HIR preparan operandos bool y enteros de 8 y 16 bits, y
desplazamientos con los enteros admitidos. LLVM promueve operandos pequeños a
`int`, preservando signo mediante la conversión existente; para desplazamientos
calcula el valor en i32 y normaliza el contador. Los accesos al arreglo C global
volatile en modo MS conservan su tratamiento anterior.

Está preparado sin construir ni validar. Faltan controles para las promociones,
contadores anchos y preservación de bool nativo. No se modificó NSLink.
