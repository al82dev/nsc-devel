# T08-015: implementación de división y desplazamientos de `_Bool` C

El analizador, HIR y el descenso LLVM admiten `/=`, `%=`, `<<=` y `>>=` con
`int` para el arreglo C global `volatile _Bool` en modo MS. Se reutiliza la
ruta de promoción i32, operación y conversión a cero/no cero, manteniendo los
accesos acquire/release de byte y sin ampliar el bool nativo.

El cambio está preparado, sin construcción ni validación. Faltan controles de
ejecución e IR; valores indefinidos para C no se declaran compatibles por
esta implementación. No modifica NSLink.
