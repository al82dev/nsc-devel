# Verificación de los casos del reporte original (2026-11-14)

Fuente: `A:/native-sharp-reference/REPORTE-PARA-WORKER-COMPILADOR-SDK.md`. Las reproducciones se
copiaron a `A:/nsc-fixes-evidencia/reporte/` (las que referenciaban `C:/native-sharp/lib` apuntan a
`lib/` de esta copia) y se compilaron con el `nsc.exe` aislado de HEAD. Las pruebas se ejecutaron
con los guardias de `binario-de-prueba-vigente.md`: cada ejecutable en proceso se reconstruyó antes
de su corrida, así que los resultados describen esta revisión.

| Caso | Reproducción original | Pruebas en esta revisión |
| --- | --- | --- |
| 39 `++`/`--` de usuario | Las dos declaraciones compilan y ejecutan 0 en O0. | `operator_increment_{decrement,binary,owned,owned_binary,owned_field,owned_field_alias}.py` pasan; no estaban registradas y ahora son `nsc.compiler.operator-increment-*`. |
| 43 override sellado | — | `nsc.compiler.sealed-override-dispatch` y `-binary` pasan. |
| 69 delegado genérico | — | `nsc.compiler.generic-delegate-return` pasa. |
| 69 lambda en `Main` global | Compila y devuelve 3 (`add(1, 2)`). | `nsc.compiler.global-lambda` pasa. |
| 69 `TOp : Reducer<T>` | Compila y ejecuta 0. | `nsc.generics.dependent-constraints` pasa. |
| 70 función externa con dimensiones const | Compila y ejecuta 0. | `nsc.compiler.fixed-matrix-const-forward` pasa. |
| 70 matriz dinámica `decimal` | Compila y ejecuta 0. | `nsc.compiler.decimal-generic-array*` pasan. |
| 70 método genérico en matriz genérica | Compila y devuelve 6 (`CellCount` de 2×3). | `nsc.generics.owner-method-matrix` y `generic_owner_method_{inference,binary,producer_trait}` pasan. |
| 70 temporal en operador `in` | Compila y ejecuta 0. | `nsc.compiler.operator-in-temporary` y `-lifetime` pasan. |
| 74 consulta de campos | — | `static_field_source_fields.py` y `static_field_binary_fields.py` pasan; no estaban registradas y ahora son `nsc.compiler.static-field-*`. |
| 88 cancelación async | Devuelve 5: la limpieza no corre al cancelar tras el primer `yield` sin avanzar. | `nsc.compiler.async-example88-deferred-cancel` pasa: con el avance siguiente la traza es 7, el contrato que el usuario confirmó. |
| 97 AddressSanitizer | — | `nsc.project-oop-address-sanitizer`, `nsc.builder-address-sanitizer-integrated-runtime`, `nsc.driver-address-sanitizer-heap-example` y las pruebas `asan` del SDK pasan. |

Límites que siguen abiertos y no forman parte de estos casos: formas de destino de clase de `++`/`--`
distintas de las cubiertas (NSC-NCSS-003), runtimes personalizados con ASan, y los casos de
`operator_increment_owned_field_throw*` y `*multi_alias*`, que necesitan una biblioteca Core y sus
metadatos como argumentos y no se ejecutaron aquí.
