# Tanda 08: dirección de elemento de arreglo C const

- Se prepara una compilación que intenta asignar `&readonly_array[1]` a `int*`. Debe rechazarse por inmutabilidad, para evitar modificar indirectamente un elemento C const.
- Estado: pendiente de validación agrupada. Si se acepta, debe diagnosticarse la ruta de addressabilidad antes de declarar soporte de `const`.
