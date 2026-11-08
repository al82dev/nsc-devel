# T08-003: referencia de objeto C generado a símbolo NativeSharp

El fixture intenta enlazar una función C `static inline` que llama a un símbolo
`export extern C` definido por NativeSharp. El consumidor exige el valor 42.
La matriz solicita ThinLTO en O0–O3 y comprueba el índice en O1–O3.

La finalidad es verificar que los objetos C añadidos tras el backend conserven
sus referencias hacia código NativeSharp. Esta prueba es una operación de una
biblioteca C llamada desde NativeSharp; no amplía el alcance al consumo general
de NativeSharp desde C++. Su ejecución y sus límites se registrarán aparte.
