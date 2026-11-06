# Callback vectorcall y variádico a través de metadata

El fixture prepara dos retornos desde biblioteca NativeSharp precompilada:
puntero a función C con vectorcall y puntero variádico C con un parámetro fijo.
El consumidor los invoca y comprueba el resultado en O0–O3. La prueba está
pendiente; no demuestra las restantes convenciones ni todas las promociones.
