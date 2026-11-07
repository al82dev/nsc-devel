# T08-015: operación compuesta `_Bool` C con `int` en LLVM

La ruta del arreglo C `volatile _Bool` promueve la lectura normalizada a i32,
aplica `+=`, `-=`, `*=`, `&=`, `|=` o `^=` con el operando `int` y convierte
el resultado a cero/no cero antes de escribir el byte. Conserva los órdenes
acquire/release y la rama ya probada de operaciones entre bool.

La implementación está preparada, sin construcción ni validación; faltan los
controles de ejecución e IR. No cubre división, desplazamientos ni otros tipos.
No modifica NSLink ni las reglas de bool nativo.
