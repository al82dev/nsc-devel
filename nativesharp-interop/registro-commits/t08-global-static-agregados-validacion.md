# T08-004: validacion de agregados y calificadores estaticos C

Construccion incremental correcta. El control C y NativeSharp pasaron O0–O3
para un record, un arreglo de dos int y globales const int y volatile int.
El intento de asignar const produjo el diagnostico de asignacion invalida.
En el IR emitido por Clang y NSC se comprobaron load acquire y store release
volatile de i32 con alineacion cuatro. Pasaron tambien las dos regresiones de
globales estaticas escalares e identidad entre productores precompilados.

Se agrega el control al gate de la tanda, que aun no se ejecuto por completo.
No se demuestra con esto soporte general de agregados, calificadores ni TLS.
