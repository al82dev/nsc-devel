# Layout de campos C++ importados

El modelo usa los tamaños, alineaciones y desplazamientos observados para tipos
y campos C++ importados, en vez de calcularlos como estructuras NativeSharp.
Verifica límites y alineación y diagnostica los tipos sin layout disponible.

La compatibilidad de todos los layouts y reglas de herencia no queda demostrada
por este cambio; requiere las pruebas de cada contrato.
