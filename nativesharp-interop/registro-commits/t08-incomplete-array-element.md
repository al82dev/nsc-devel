# Tanda 08: elemento de arreglos incompletos

`describeType` obtiene el elemento de cualquier arreglo reconocido por Clang,
incluidos los incompletos; la cantidad solo se registra cuando Clang la conoce.
Esto conserva el tipo del elemento sin inventar un tamaño. Es una captura de
metadatos: la proyección y el uso desde NativeSharp siguen pendientes.

La construcción y las pruebas se ejecutarán junto con la validación de la tanda.
