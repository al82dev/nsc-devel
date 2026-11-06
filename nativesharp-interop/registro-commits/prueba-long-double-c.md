# Contrato de long double C

Se prepara una prueba C que exige tamaño, precisión y rango de `long double`
iguales a `double`, y un consumidor NativeSharp que usa typedef, argumentos,
resultado y campo. También se genera IR de Clang. La matriz está pendiente;
no se infiere compatibilidad en otros destinos o con otro formato flotante.
