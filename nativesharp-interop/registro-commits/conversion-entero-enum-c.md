# Conversión de entero a enum C importado

La prueba identificó rechazos de `builtin:int` en inicialización, asignación
y argumentos de enum C. Se prepara la conversión numérica cuando el destino
es un enum importado de C con representación válida; la clasificación no
afecta enums NativeSharp. LLVM ya usa el ancho y signo del enum para convertir.
Falta comprobar ejecución, anchos, argumentos y regresiones agrupadas.
