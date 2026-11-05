# Tanda 08: enum usado únicamente por un global

- Se prepara `hidden_global`, cuyo enum no se nombra en NativeSharp ni aparece en los parámetros o resultados de las funciones C usadas. NativeSharp lee el global y compara su valor con el retorno entero de C.
- El caso distingue la selección del tipo por demanda del global de la selección ya existente por firmas de función. Pendiente de validación agrupada.
