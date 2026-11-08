# T08 H3: control de dependencia C fuera del bloque sombreado

Se agrega un productor genérico y su consumidor precompilado para distinguir el
símbolo C `shadow_function` de una variable local con el mismo nombre. La
plantilla llama la función antes y después de un bloque anidado que declara la
variable local; el resultado esperado es 81 + 17 + 81 = 179.

Este fixture complementa el control negativo de capturas indebidas: una futura
corrección no debe eliminar la dependencia C por el solo hecho de encontrar
un identificador local homónimo. Su ejecución se registrará por separado.
