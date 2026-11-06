# Catálogo precompilado y encabezado independiente

Se añade un consumidor que combina un tipo C de la biblioteca NativeSharp
precompilada con otro tipo C expuesto por un encabezado y una sesión Clang propios.
La prueba ejerce campos y punteros de ambos y está prevista de O0 a O3. Separa
la observación de catálogos del caso todavía no resuelto de dos importaciones del
mismo nombre. La primera ejecución alcanzó el enlace en O0, pero falló con
NSL1021 porque el mismo archivo `api.lib` entraba por ambas bibliotecas. Se
separaron los objetos y las bibliotecas C de cada encabezado; falta repetir la
prueba agrupada. No acredita reconciliación de identidades.
