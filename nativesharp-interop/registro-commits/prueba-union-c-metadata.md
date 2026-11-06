# Union C compartida mediante metadata

Se preparó una prueba con productor NativeSharp precompilado y consumidor que
importan el mismo encabezado C. Comprueba identidad del puntero y transmisión
por valor de la union. La matriz completa de metadata pasó O0–O3, incluidos
estos casos y el rechazo de encabezado cambiado. Esto cubre una union concreta,
no todas las formas de conciliación o layout.
