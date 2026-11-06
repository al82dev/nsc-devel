# Conversión de entero a enum C importado

La prueba identificó rechazos de `builtin:int` en inicialización, asignación
y argumentos de enum C. Se prepara la conversión numérica cuando el destino
es un enum importado de C con representación válida; la clasificación no
afecta enums NativeSharp. LLVM ya usa el ancho y signo del enum para convertir.
Falta comprobar ejecución, anchos, argumentos y regresiones agrupadas.

La construcción incremental `213e20187e5ead31` pasó las pruebas reales de
inicialización, asignación y argumentos O0–O3, incluidos entero variable,
truncamiento 64→32 y extensión con signo 8→32. Un enum NativeSharp rechazó
la inicialización entera, como antes. El arreglo de enum C se inicializó desde
NativeSharp y pasó O0–O3. La suite de sesión pasó 1/1; la compilación produjo
32 advertencias de inicializadores de la suite. Esto no cierra el soporte de
todos los enums ni prueba todos los valores o conversiones entre enums.
