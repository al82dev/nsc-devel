# Tanda 08: propagación de volatile en arreglos globales C

- La proyección conserva `volatile` y `const` de los niveles de arreglo o de su elemento escalar/enum. La dirección indexada de arreglo fijo conserva ese indicador; la carga por puntero lo transmite a LLVM y la escritura utiliza el indicador ya reconocido por `storeFromParts`.
- La propagación no interpreta un puntero volatile como puntero a elemento volatile. Siguen pendientes pruebas agrupadas de arreglo, matriz, IR y regresiones; no se declara cerrado T08-015.
