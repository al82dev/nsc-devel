# Tanda 08: fixture de arreglo C volatile enum

- NativeSharp prepara una escritura y lectura de un arreglo global C `volatile enum`; C verifica el valor. El script comprueba que ambas operaciones del símbolo estén marcadas volatile en LLVM IR.
- Estado: pendiente de validación agrupada. El caso no acredita todas las variantes de enum o calificadores.
