# T08-003: corrupción de sidecar y referencias nativas

Con `nsc` de desarrollo build `c1d4e389ef03ddc9`, el validador de funciones C
inline terminó con código 0 mediante `call`. Además de su matriz O0–O3, alteró
la huella de un sidecar v3 y después de ambos: en O3 el consumidor ejecutó
correctamente, informó una y después cero bibliotecas ThinLTO y registró el
fallback de cada sidecar dañado. Las copias temporales originales se restauraron.

El control de símbolos también pasó mediante `call` en O0–O3. Una biblioteca
precompilada contiene una función C inline que llama al `HostCallback` exportado
por NativeSharp; su consumidor separado obtuvo 42. En O1–O3 se exigió el consumo
de un sidecar ThinLTO. La misma prueba directa también pasó.

Los controles acreditan estas formas concretas; no cubren toda corrupción
autenticada, todas las referencias nativas ni el cierre de la tanda 8.
