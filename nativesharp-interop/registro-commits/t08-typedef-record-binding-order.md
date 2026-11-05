# Tanda 08: orden del índice de records importados

El índice `cppImportedType` se finaliza después del enlace de nombres. Para el
caso `typedef struct T T`, la comprobación consulta ahora la declaración ya
agregada y su entrada importada, sin depender de ese índice todavía vacío.
La prueba integrada preparada en `t08-typedef-record-binding-tests` queda
pendiente de la validación agrupada.
