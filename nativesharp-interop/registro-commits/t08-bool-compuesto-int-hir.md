# T08-015: HIR conserva operando `int` de `_Bool` C compuesto

El descenso conserva el operando `int` sin convertirlo anticipadamente a bool
para los seis operadores admitidos en un índice directo del arreglo C global
`volatile _Bool` identificado por su marcador de modo MS y alineación de byte.
La ruta general de conversión permanece igual.

Esto está preparado sin construir ni validar. Aún falta operar en i32 en LLVM,
convertir el resultado a bool y ejecutar los controles agrupados; no constituye
soporte completo y no modifica NSLink.
